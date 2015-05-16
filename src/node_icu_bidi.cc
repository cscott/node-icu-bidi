#include <cstring> // for std::memcpy

#include "nodes.h"
#include "unicode/ubidi.h"

#include "macros.h"

using namespace v8;

/* A type-safe improvement on node::SetPrototypeMethod */
template <typename target_t>
static void bidi_SetPrototypeMethod(target_t target, const char *name,
                             INVOCATION_CALLBACK_TYPE callback) {
	CREATE_ISOLATE_CONTEXT;

  Local<FunctionTemplate> templ = NEW_FUNCTION_TEMPLATE_3(
    callback, UNDEFINED, NEW_SIGNATURE(target)
  );
  target->PrototypeTemplate()->Set(NEW_SYMBOL(name), templ);  
}

static Handle<Value> dir2str(UBiDiDirection dir) {
	CREATE_ISOLATE_CONTEXT;
	CREATE_ESCAPABLE_SCOPE;
  
  if(dir == UBIDI_LTR)
    return CLOSE_SCOPE(NEW_SYMBOL("ltr"));
  else if(dir == UBIDI_RTL)
    return CLOSE_SCOPE(NEW_SYMBOL("rtl"));
  else if(dir == UBIDI_MIXED)
    return CLOSE_SCOPE(NEW_SYMBOL("mixed"));
  else if(dir == UBIDI_NEUTRAL)
    return CLOSE_SCOPE(NEW_SYMBOL("neutral"));
  else
    return CLOSE_SCOPE(NEW_SYMBOL("undefined"));
}

static UBiDiDirection level2dir(UBiDiLevel level) {
    return (level&1) ? UBIDI_RTL : UBIDI_LTR;
}

#define CHECK_UBIDI_ERR(obj)                                        \
  do {                                                              \
    if (U_FAILURE((obj)->errorCode)) {                              \
      RETURN_THROW_EXCEPTION("The bidi algorithm failed");  \
    }                                                               \
  } while (false)

// declarations
class Paragraph : public node::ObjectWrap {
public:
  static Persistent<FunctionTemplate> constructor_template;
  static Persistent<Function> constructor;

  static void Init(Handle<Object> target);
protected:
  Paragraph() : node::ObjectWrap(),
                para(NULL),
                text(NULL),
                runs(-1),
                errorCode(U_ZERO_ERROR),
                parent()  {
  }
  ~Paragraph() {
    if (para != NULL) {
      ubidi_close(para);
      para = NULL;
    }
    if (text != NULL) {
      delete text;
      text = NULL;
    }
    // this is parent.Reset() in newer v8
    DISPOSE_PERSISTENT(parent);
    //parent.Clear();
  }

  static METHOD_RETURN_TYPE New(const ARGS_TYPE& args);

  static METHOD_RETURN_TYPE GetDirection(const ARGS_TYPE& args);
  static METHOD_RETURN_TYPE GetParaLevel(const ARGS_TYPE& args);
  static METHOD_RETURN_TYPE GetLevelAt(const ARGS_TYPE& args);
  static METHOD_RETURN_TYPE GetLength(const ARGS_TYPE& args);
  static METHOD_RETURN_TYPE GetProcessedLength(const ARGS_TYPE& args);
  static METHOD_RETURN_TYPE GetResultLength(const ARGS_TYPE& args);
  static METHOD_RETURN_TYPE GetVisualIndex(const ARGS_TYPE& args);
  static METHOD_RETURN_TYPE GetLogicalIndex(const ARGS_TYPE& args);

  static METHOD_RETURN_TYPE CountParagraphs(const ARGS_TYPE& args);
  static METHOD_RETURN_TYPE GetParagraph(const ARGS_TYPE& args);
  static METHOD_RETURN_TYPE GetParagraphByIndex(const ARGS_TYPE& args);

  static METHOD_RETURN_TYPE CountRuns(const ARGS_TYPE& args);
  static METHOD_RETURN_TYPE GetVisualRun(const ARGS_TYPE& args);
  static METHOD_RETURN_TYPE GetLogicalRun(const ARGS_TYPE& args);

  static METHOD_RETURN_TYPE SetLine(const ARGS_TYPE& args);
  static METHOD_RETURN_TYPE WriteReordered(const ARGS_TYPE& args);

protected:
  UBiDi *para;
  UChar *text;
  int32_t runs;
  UErrorCode errorCode;
  // keep a pointer to the parent paragraph to ensure that lines are
  // gc'ed/destroyed before the paragraph they belong to.
  Persistent<Object> parent;
};

// implementation
Persistent<Function> Paragraph::constructor;
Persistent<FunctionTemplate> Paragraph::constructor_template;

void Paragraph::Init(Handle<Object> target) {
	CREATE_ISOLATE_CONTEXT;
  const char *CLASS_NAME = "Paragraph";

  Local<FunctionTemplate> t = NEW_FUNCTION_TEMPLATE(New);

  t->InstanceTemplate()->SetInternalFieldCount(1);
  t->SetClassName(NEW_STRING(CLASS_NAME));

  bidi_SetPrototypeMethod(t, "countRuns", CountRuns);
  bidi_SetPrototypeMethod(t, "getVisualRun", GetVisualRun);
  bidi_SetPrototypeMethod(t, "getLogicalRun", GetLogicalRun);

  bidi_SetPrototypeMethod(t, "getDirection", GetDirection);
  bidi_SetPrototypeMethod(t, "getParaLevel", GetParaLevel);
  bidi_SetPrototypeMethod(t, "getLevelAt", GetLevelAt);
  bidi_SetPrototypeMethod(t, "getLength", GetLength);
  bidi_SetPrototypeMethod(t, "getProcessedLength", GetProcessedLength);
  bidi_SetPrototypeMethod(t, "getResultLength", GetResultLength);

  bidi_SetPrototypeMethod(t, "getVisualIndex", GetVisualIndex);
  bidi_SetPrototypeMethod(t, "getLogicalIndex", GetLogicalIndex);

  bidi_SetPrototypeMethod(t, "countParagraphs", CountParagraphs);
  bidi_SetPrototypeMethod(t, "getParagraph", GetParagraph);
  bidi_SetPrototypeMethod(t, "getParagraphByIndex", GetParagraphByIndex);

  bidi_SetPrototypeMethod(t, "setLine", SetLine);

  bidi_SetPrototypeMethod(t, "writeReordered", WriteReordered);

	SET_CONSTRUCTOR(constructor, t);
  SET_CONSTRUCTOR_TEMPLATE(constructor_template,t);
  SET_CONSTRUCTOR_EXPORT(target, CLASS_NAME, t);              

}

METHOD_RETURN_TYPE Paragraph::New(const ARGS_TYPE& args) {
	CREATE_ISOLATE_CONTEXT;
	CREATE_ESCAPABLE_SCOPE;

  Local<Value> result;

  if (!args.IsConstructCall()) {
    // Invoked as plain function, turn into construct call
    const int argc = args.Length();
    Local<Value> *argv = new Local<Value>[argc];
    for (int i=0; i<argc; i++) { argv[i] = args[i]; }
    Handle<Value> result = NEW_INSTANCE_ARGS(constructor,argc, argv);
    delete[] argv;
    SET_FUNCTION_RETURN_VALUE_AND_RETURN(result);
  }
  REQUIRE_ARGUMENTS(1);

  if (args[0]->IsExternal()) {
    // special back door used to create line objects.
    Paragraph *line = (Paragraph *) Local<External>::Cast(args[0])->Value();
    line->Wrap(args.This());
    SET_FUNCTION_RETURN_VALUE_AND_RETURN(args.This());
  }

  String::Value text(args[0]);
  if (*text == NULL) {
    // conversion failed.  toString() threw an exception?
   RETURN_THROW_EXCEPTION(
        "First argument couldn't be converted to a string"
    );
  }

  // an optional options hash for the second argument.
  Handle<Object> options;
  if (args.Length() <= 1) {
    options = NEW_OBJECT;
  } else {
    options = args[1]->ToObject();
    if (options.IsEmpty() || !args[1]->IsObject()) {
      RETURN_THROW_EXCEPTION(
        "Second argument should be an options hash"
      );
    }
  }

  Paragraph *para = new Paragraph();
  para->Wrap(args.This());

  para->para = ubidi_openSized(text.length(), 0, &para->errorCode);
  CHECK_UBIDI_ERR(para);
  if (para->para==NULL) {
    RETURN_THROW_EXCEPTION("libicu open failed");
  }

  // parse values from the options hash
  Handle<Value> paraLevelObj =
    options->Get(NEW_SYMBOL("paraLevel"));
  UBiDiLevel paraLevel =
    paraLevelObj->IsNumber() ? paraLevelObj->Int32Value() :
    UBIDI_DEFAULT_LTR;
  if (!(paraLevel <= UBIDI_MAX_EXPLICIT_LEVEL ||
        paraLevel == UBIDI_DEFAULT_LTR ||
        paraLevel == UBIDI_DEFAULT_RTL)) {
    paraLevel = UBIDI_DEFAULT_LTR;
  }

  Handle<Value> reorderingModeObj =
    options->Get(NEW_SYMBOL("reorderingMode"));
  UBiDiReorderingMode reorderingMode =
    reorderingModeObj->IsNumber() ?
    (UBiDiReorderingMode) reorderingModeObj->Int32Value() :
    UBIDI_REORDER_COUNT;
  if (reorderingMode >= 0 && reorderingMode < UBIDI_REORDER_COUNT) {
    ubidi_setReorderingMode(para->para, reorderingMode);
  }

  Handle<Value> reorderingOptionsObj =
    options->Get(NEW_SYMBOL("reorderingOptions"));
  int32_t reorderingOptions =
    reorderingOptionsObj->IsNumber() ? reorderingOptionsObj->Int32Value() :
    -1;
  if (reorderingOptions >= 0 && reorderingOptions <=
      (UBIDI_OPTION_INSERT_MARKS | UBIDI_OPTION_REMOVE_CONTROLS |
       UBIDI_OPTION_STREAMING)) {
    ubidi_setReorderingOptions(para->para, (uint32_t) reorderingOptions);
  }

  Handle<Value> inverseObj =
    options->Get(NEW_SYMBOL("inverse"));
  if (inverseObj->IsBoolean()) {
    ubidi_setInverse(para->para, inverseObj->BooleanValue());
  }

  Handle<Value> reorderParagraphsLTRObj =
    options->Get(NEW_SYMBOL("reorderParagraphsLTR"));
  if (reorderParagraphsLTRObj->IsBoolean()) {
    ubidi_orderParagraphsLTR(para->para, reorderParagraphsLTRObj->BooleanValue());
  }

  Handle<Value> prologueObj =
    options->Get(NEW_SYMBOL("prologue"));
  Handle<Value> epilogueObj =
    options->Get(NEW_SYMBOL("epilogue"));
  String::Value prologue(prologueObj);
  String::Value epilogue(epilogueObj);
  int32_t plen = prologueObj->IsString() ? prologue.length() : 0;
  int32_t elen = epilogueObj->IsString() ? epilogue.length() : 0;

  // Copy main, prologue, and epilogue text and keep it alive as long
  // as we're alive.
  para->text = new UChar[plen + text.length() + elen];
  std::memcpy(para->text, *prologue, plen * sizeof(UChar));
  std::memcpy(para->text + plen, *text, text.length() * sizeof(UChar));
  std::memcpy(para->text + plen + text.length(), *epilogue, elen * sizeof(UChar));
  if (plen!=0 || elen!=0) {
    ubidi_setContext(
      para->para, para->text, plen,
      para->text + plen + text.length(), elen,
      &para->errorCode
    );
    CHECK_UBIDI_ERR(para);
  }

  // XXX parse options.embeddingLevels and construct an appropriate array of
  //     UBiDiLevel to pass to setPara

  ubidi_setPara(para->para, para->text + plen, text.length(),
                paraLevel, NULL, &para->errorCode);
  CHECK_UBIDI_ERR(para);

  SET_FUNCTION_RETURN_VALUE(args.This());
}

METHOD_RETURN_TYPE Paragraph::SetLine(const ARGS_TYPE& args) {
	CREATE_ISOLATE_CONTEXT;
	CREATE_ESCAPABLE_SCOPE;

  Paragraph *para = node::ObjectWrap::Unwrap<Paragraph>(args.Holder());
  if (!para->parent.IsEmpty()) {
    RETURN_THROW_EXCEPTION(
      "This is already a line"
    );
  }
  REQUIRE_ARGUMENT_NUMBER(0);
  REQUIRE_ARGUMENT_NUMBER(1);
  int32_t start = args[0]->Int32Value();
  int32_t limit = args[1]->Int32Value();

  // Create para object.
  Paragraph *line = new Paragraph();
  line->para = ubidi_openSized(ubidi_getLength(para->para), 0, &line->errorCode);
  if (U_FAILURE(line->errorCode) || line->para==NULL) {
    delete line;
    RETURN_THROW_EXCEPTION("libicu open failed");
  }

  ubidi_setLine(para->para, start, limit, line->para, &line->errorCode);
  if (U_FAILURE(line->errorCode)) {
    delete line;
    RETURN_THROW_EXCEPTION("setLine failed");
  }

  // okay, now create a js object wrapping this Paragraph
  // indicate to the constructor that we just want to wrap this object
  // by passing it as an External
  Local<Value> consArgs[1] = { NEW_EXTERNAL(line) };
  Local<Object> lineObj = NEW_INSTANCE_ARGS(constructor, 1, consArgs);

  SET_PERSISTENT_OBJECT((line->parent) , Object , args.Holder());
  SET_FUNCTION_RETURN_VALUE(lineObj); 
}

METHOD_RETURN_TYPE Paragraph::GetParaLevel(const ARGS_TYPE& args) {
	CREATE_ISOLATE_CONTEXT;
	CREATE_ESCAPABLE_SCOPE;

  Paragraph *para = node::ObjectWrap::Unwrap<Paragraph>(args.Holder());
  SET_FUNCTION_RETURN_VALUE(NEW_INTEGER(ubidi_getParaLevel(para->para)));
}

METHOD_RETURN_TYPE Paragraph::GetLevelAt(const ARGS_TYPE& args) {
	CREATE_ISOLATE_CONTEXT;
	CREATE_ESCAPABLE_SCOPE;

  Paragraph *para = node::ObjectWrap::Unwrap<Paragraph>(args.Holder());
  REQUIRE_ARGUMENT_NUMBER(0);
  int32_t charIndex = args[0]->Int32Value();
  SET_FUNCTION_RETURN_VALUE(NEW_INTEGER(ubidi_getLevelAt(para->para, charIndex)));
}

METHOD_RETURN_TYPE Paragraph::CountParagraphs(const ARGS_TYPE& args) {
	CREATE_ISOLATE_CONTEXT;
	CREATE_ESCAPABLE_SCOPE;

  Paragraph *para = node::ObjectWrap::Unwrap<Paragraph>(args.Holder());
  SET_FUNCTION_RETURN_VALUE(NEW_INTEGER(ubidi_countParagraphs(para->para)));
}

METHOD_RETURN_TYPE Paragraph::GetDirection(const ARGS_TYPE& args) {
	CREATE_ISOLATE_CONTEXT;
	CREATE_ESCAPABLE_SCOPE;

  Paragraph *para = node::ObjectWrap::Unwrap<Paragraph>(args.Holder());
  UBiDiDirection dir = ubidi_getDirection(para->para);
  SET_FUNCTION_RETURN_VALUE(dir2str(dir));
}

METHOD_RETURN_TYPE Paragraph::GetLength(const ARGS_TYPE& args) {
	CREATE_ISOLATE_CONTEXT;
	CREATE_ESCAPABLE_SCOPE;
  
  Paragraph *para = node::ObjectWrap::Unwrap<Paragraph>(args.Holder());
  SET_FUNCTION_RETURN_VALUE(NEW_INTEGER(ubidi_getLength(para->para)));
}

METHOD_RETURN_TYPE Paragraph::GetProcessedLength(const ARGS_TYPE& args) {
	CREATE_ISOLATE_CONTEXT;
	CREATE_ESCAPABLE_SCOPE;
  
  Paragraph *para = node::ObjectWrap::Unwrap<Paragraph>(args.Holder());
  SET_FUNCTION_RETURN_VALUE(NEW_INTEGER(ubidi_getProcessedLength(para->para)));
}

METHOD_RETURN_TYPE Paragraph::GetResultLength(const ARGS_TYPE& args) {
	CREATE_ISOLATE_CONTEXT;
	CREATE_ESCAPABLE_SCOPE;
  Paragraph *para = node::ObjectWrap::Unwrap<Paragraph>(args.Holder());
  SET_FUNCTION_RETURN_VALUE(NEW_INTEGER(ubidi_getResultLength(para->para)));
}

METHOD_RETURN_TYPE Paragraph::GetVisualIndex(const ARGS_TYPE& args) {
	CREATE_ISOLATE_CONTEXT;
	CREATE_ESCAPABLE_SCOPE;
  
  Paragraph *para = node::ObjectWrap::Unwrap<Paragraph>(args.Holder());
  REQUIRE_ARGUMENT_NUMBER(0);
  int32_t logicalIndex = args[0]->Int32Value();
  int32_t visualIndex =
    ubidi_getVisualIndex(para->para, logicalIndex, &para->errorCode);
  CHECK_UBIDI_ERR(para);
  SET_FUNCTION_RETURN_VALUE(NEW_INTEGER(visualIndex));
}

METHOD_RETURN_TYPE Paragraph::GetLogicalIndex(const ARGS_TYPE& args) {
	CREATE_ISOLATE_CONTEXT;
	CREATE_ESCAPABLE_SCOPE;
  Paragraph *para = node::ObjectWrap::Unwrap<Paragraph>(args.Holder());
  REQUIRE_ARGUMENT_NUMBER(0);
  int32_t visualIndex = args[0]->Int32Value();
  int32_t logicalIndex =
    ubidi_getLogicalIndex(para->para, visualIndex, &para->errorCode);
  CHECK_UBIDI_ERR(para);
  SET_FUNCTION_RETURN_VALUE(NEW_INTEGER(logicalIndex));
}

METHOD_RETURN_TYPE Paragraph::CountRuns(const ARGS_TYPE& args) {
	CREATE_ISOLATE_CONTEXT;
	CREATE_ESCAPABLE_SCOPE;
  Paragraph *para = node::ObjectWrap::Unwrap<Paragraph>(args.Holder());
  if (para->runs < 0) {
    para->runs = ubidi_countRuns(para->para, &para->errorCode);
    CHECK_UBIDI_ERR(para);
  }
  SET_FUNCTION_RETURN_VALUE(NEW_INTEGER(para->runs));
}

METHOD_RETURN_TYPE Paragraph::GetVisualRun(const ARGS_TYPE& args) {
	CREATE_ISOLATE_CONTEXT;
	CREATE_ESCAPABLE_SCOPE;
  Paragraph *para = node::ObjectWrap::Unwrap<Paragraph>(args.Holder());
  if (para->runs < 0) {
    para->runs = ubidi_countRuns(para->para, &para->errorCode);
    CHECK_UBIDI_ERR(para);
  }
  REQUIRE_ARGUMENT_NUMBER(0);
  int32_t runIndex = args[0]->Int32Value(), logicalStart, length;
  if (!(runIndex >= 0 && runIndex < para->runs)) {
    RETURN_THROW_EXCEPTION(
      "Run index out of bounds"
    );
  }
  UBiDiDirection dir = ubidi_getVisualRun(
    para->para, runIndex, &logicalStart, &length
  );
  Local<Object> result = NEW_OBJECT;
  result->Set(NEW_SYMBOL("dir"), dir2str(dir));
  result->Set(NEW_SYMBOL("logicalStart"), NEW_INTEGER(logicalStart));
  result->Set(NEW_SYMBOL("length"), NEW_INTEGER(length));
  SET_FUNCTION_RETURN_VALUE(result);
}

METHOD_RETURN_TYPE Paragraph::GetLogicalRun(const ARGS_TYPE& args) {
	CREATE_ISOLATE_CONTEXT;
	CREATE_ESCAPABLE_SCOPE;
  
  Paragraph *para = node::ObjectWrap::Unwrap<Paragraph>(args.Holder());
  if (para->runs < 0) {
    para->runs = ubidi_countRuns(para->para, &para->errorCode);
    CHECK_UBIDI_ERR(para);
  }
  REQUIRE_ARGUMENT_NUMBER(0);
  int32_t logicalPosition = args[0]->Int32Value(), logicalLimit;
  UBiDiLevel level;
  ubidi_getLogicalRun(
    para->para, logicalPosition, &logicalLimit, &level
  );
  Local<Object> result = NEW_OBJECT;
  result->Set(NEW_SYMBOL("logicalLimit"), NEW_INTEGER(logicalLimit));
  result->Set(NEW_SYMBOL("level"), NEW_INTEGER(level));
  result->Set(NEW_SYMBOL("dir"), dir2str(level2dir(level)));
  SET_FUNCTION_RETURN_VALUE(result);
}

METHOD_RETURN_TYPE Paragraph::GetParagraph(const ARGS_TYPE& args) {
	CREATE_ISOLATE_CONTEXT;
	CREATE_ESCAPABLE_SCOPE;

  Paragraph *para = node::ObjectWrap::Unwrap<Paragraph>(args.Holder());
  REQUIRE_ARGUMENT_NUMBER(0);
  int32_t charIndex = args[0]->Int32Value(), paraStart, paraLimit;
  UBiDiLevel paraLevel;
  int32_t paraIndex = ubidi_getParagraph(
    para->para, charIndex, &paraStart, &paraLimit, &paraLevel, &para->errorCode
  );
  CHECK_UBIDI_ERR(para);

  Local<Object> result = NEW_OBJECT;
  result->Set(NEW_SYMBOL("index"), NEW_INTEGER(paraIndex));
  result->Set(NEW_SYMBOL("start"), NEW_INTEGER(paraStart));
  result->Set(NEW_SYMBOL("limit"), NEW_INTEGER(paraLimit));
  result->Set(NEW_SYMBOL("level"), NEW_INTEGER(paraLevel));
  result->Set(NEW_SYMBOL("dir"), dir2str(level2dir(paraLevel)));
  SET_FUNCTION_RETURN_VALUE(result);
}

METHOD_RETURN_TYPE Paragraph::GetParagraphByIndex(const ARGS_TYPE& args) {
	CREATE_ISOLATE_CONTEXT;
	CREATE_ESCAPABLE_SCOPE;
  
  Paragraph *para = node::ObjectWrap::Unwrap<Paragraph>(args.Holder());
  REQUIRE_ARGUMENT_NUMBER(0);
  int32_t paraIndex = args[0]->Int32Value(), paraStart, paraLimit;
  UBiDiLevel paraLevel;
  ubidi_getParagraphByIndex(
    para->para, paraIndex, &paraStart, &paraLimit, &paraLevel, &para->errorCode
  );
  CHECK_UBIDI_ERR(para);

  Local<Object> result = NEW_OBJECT;
  result->Set(NEW_SYMBOL("index"), NEW_INTEGER(paraIndex));
  result->Set(NEW_SYMBOL("start"), NEW_INTEGER(paraStart));
  result->Set(NEW_SYMBOL("limit"), NEW_INTEGER(paraLimit));
  result->Set(NEW_SYMBOL("level"), NEW_INTEGER(paraLevel));
  result->Set(NEW_SYMBOL("dir"), dir2str(level2dir(paraLevel)));
  SET_FUNCTION_RETURN_VALUE(result);
}

METHOD_RETURN_TYPE Paragraph::WriteReordered(const ARGS_TYPE& args) {
	CREATE_ISOLATE_CONTEXT;
	CREATE_ESCAPABLE_SCOPE;
  
  Paragraph *para = node::ObjectWrap::Unwrap<Paragraph>(args.Holder());
  uint16_t options = 0;
  if (args.Length() > 0) {
    REQUIRE_ARGUMENT_NUMBER(0);
    options = (uint16_t) args[0]->Uint32Value();
  }
  // Allocate a buffer to hold the result.
  int32_t destSize = ubidi_getProcessedLength(para->para);
  if (ubidi_getLength(para->para) > destSize) {
    destSize = ubidi_getLength(para->para);
  }
  if (0 != (options & UBIDI_INSERT_LRM_FOR_NUMERIC)) {
    destSize += 2 * ubidi_countRuns(para->para, &para->errorCode);
    CHECK_UBIDI_ERR(para);
  }
  UChar dest[destSize];
  int32_t resultSize = ubidi_writeReordered(
    para->para, dest, destSize, options, &para->errorCode
  );
  CHECK_UBIDI_ERR(para);
  if (resultSize > destSize) {
    RETURN_THROW_EXCEPTION(
        "Allocation error (this should never happen)"
    );
  }
  SET_FUNCTION_RETURN_VALUE(NEW_STRING_FROM_BUFFER(dest, resultSize));
}


// Tell node about our module!
void RegisterModule(Handle<Object> exports) {
  CREATE_ISOLATE_CONTEXT;

  Paragraph::Init(exports);

  DEFINE_CONSTANT_INTEGER(exports, UBIDI_LTR, LTR);
  DEFINE_CONSTANT_INTEGER(exports, UBIDI_RTL, RTL);
  DEFINE_CONSTANT_INTEGER(exports, UBIDI_DEFAULT_LTR, DEFAULT_LTR);
  DEFINE_CONSTANT_INTEGER(exports, UBIDI_DEFAULT_RTL, DEFAULT_RTL);
  DEFINE_CONSTANT_INTEGER(exports, UBIDI_MAX_EXPLICIT_LEVEL, MAX_EXPLICIT_LEVEL);
  DEFINE_CONSTANT_INTEGER(exports, UBIDI_LEVEL_OVERRIDE, LEVEL_OVERRIDE);
  DEFINE_CONSTANT_INTEGER(exports, UBIDI_MAP_NOWHERE, MAP_NOWHERE);

  // Reordered.<constant>: option bits for writeReordered
  Handle<Object> re = NEW_OBJECT;
  exports->SET_WITH_ATTRIBUTES_METHOD(NEW_SYMBOL("Reordered"), re,
               static_cast<PropertyAttribute>(ReadOnly | DontDelete));
  DEFINE_CONSTANT_INTEGER(re, UBIDI_KEEP_BASE_COMBINING, KEEP_BASE_COMBINING);
  DEFINE_CONSTANT_INTEGER(re, UBIDI_DO_MIRRORING, DO_MIRRORING);
  DEFINE_CONSTANT_INTEGER(re, UBIDI_INSERT_LRM_FOR_NUMERIC, INSERT_LRM_FOR_NUMERIC);
  DEFINE_CONSTANT_INTEGER(re, UBIDI_REMOVE_BIDI_CONTROLS, REMOVE_BIDI_CONTROLS);
  DEFINE_CONSTANT_INTEGER(re, UBIDI_OUTPUT_REVERSE, OUTPUT_REVERSE);

  // ReorderingMode.<constant>
  Handle<Object> rm = NEW_OBJECT;
  exports->SET_WITH_ATTRIBUTES_METHOD(NEW_SYMBOL("ReorderingMode"), rm,
               static_cast<PropertyAttribute>(ReadOnly | DontDelete));
  DEFINE_CONSTANT_INTEGER(rm, UBIDI_REORDER_DEFAULT, DEFAULT);
  DEFINE_CONSTANT_INTEGER(rm, UBIDI_REORDER_NUMBERS_SPECIAL, NUMBERS_SPECIAL);
  DEFINE_CONSTANT_INTEGER(rm, UBIDI_REORDER_GROUP_NUMBERS_WITH_R, GROUP_NUMBERS_WITH_R);
  DEFINE_CONSTANT_INTEGER(rm, UBIDI_REORDER_RUNS_ONLY, RUNS_ONLY);
  DEFINE_CONSTANT_INTEGER(rm, UBIDI_REORDER_INVERSE_NUMBERS_AS_L, INVERSE_NUMBERS_AS_L);
  DEFINE_CONSTANT_INTEGER(rm, UBIDI_REORDER_INVERSE_LIKE_DIRECT, INVERSE_LIKE_DIRECT);
  DEFINE_CONSTANT_INTEGER(rm, UBIDI_REORDER_INVERSE_FOR_NUMBERS_SPECIAL, INVERSE_FOR_NUMBERS_SPECIAL);
  //DEFINE_CONSTANT_INTEGER(rm, UBIDI_REORDER_COUNT, COUNT); //number of enums

  Handle<Object> ro = NEW_OBJECT;
  exports->SET_WITH_ATTRIBUTES_METHOD(NEW_SYMBOL("ReorderingOption"), ro,
               static_cast<PropertyAttribute>(ReadOnly | DontDelete));
  DEFINE_CONSTANT_INTEGER(ro, UBIDI_OPTION_DEFAULT, DEFAULT);
  DEFINE_CONSTANT_INTEGER(ro, UBIDI_OPTION_INSERT_MARKS, INSERT_MARKS);
  DEFINE_CONSTANT_INTEGER(ro, UBIDI_OPTION_REMOVE_CONTROLS, REMOVE_CONTROLS);
  DEFINE_CONSTANT_INTEGER(ro, UBIDI_OPTION_STREAMING, STREAMING);

}

NODE_MODULE(icu_bidi, RegisterModule)

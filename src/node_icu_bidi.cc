#include <node.h>
#include <v8.h>
#include <cstring> // for std::memcpy

#include "unicode/ubidi.h"

#include "macros.h"

using namespace v8;

/* A type-safe improvement on node::SetPrototypeMethod */
template <typename target_t>
static void bidi_SetPrototypeMethod(target_t target, const char *name,
                             v8::InvocationCallback callback) {
  Local<FunctionTemplate> templ = FunctionTemplate::New(
    callback, Undefined(), Signature::New(target)
  );
  target->PrototypeTemplate()->Set(String::NewSymbol(name), templ);
}

static Handle<Value> bidi_MakeException(UErrorCode code) {
  // xxx: stash the error code somewhere in the exception object
  return Exception::Error(String::New("The bidi algorithm failed"));
}

static Handle<Value> dir2str(UBiDiDirection dir) {
  HandleScope scope;
  return scope.Close(dir == UBIDI_LTR ? String::NewSymbol("ltr") :
                     dir == UBIDI_RTL ? String::NewSymbol("rtl") :
                     dir == UBIDI_MIXED ? String::NewSymbol("mixed") :
                     dir == UBIDI_NEUTRAL ? String::NewSymbol("neutral") :
                     Undefined() /* should never happen */);
}

static UBiDiDirection level2dir(UBiDiLevel level) {
    return (level&1) ? UBIDI_RTL : UBIDI_LTR;
}

#define CHECK_UBIDI_ERR(obj)                                        \
  do {                                                              \
    if (U_FAILURE((obj)->errorCode)) {                              \
      return ThrowException(bidi_MakeException((obj)->errorCode));  \
    }                                                               \
  } while (false)

// declarations
class Paragraph : public node::ObjectWrap {
public:
  static Persistent<FunctionTemplate> constructor_template;
  static void Init(Handle<Object> target);
protected:
  Paragraph() : node::ObjectWrap(),
                para(NULL),
                text(NULL),
                runs(-1),
                errorCode(U_ZERO_ERROR),
                parent(Persistent<Object>())  {
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
    parent.Dispose();
    parent.Clear();
  }

  static Handle<Value> New(const Arguments& args);

  static Handle<Value> GetDirection(const Arguments &args);
  static Handle<Value> GetParaLevel(const Arguments &args);
  static Handle<Value> GetLevelAt(const Arguments &args);
  static Handle<Value> GetLength(const Arguments &args);
  static Handle<Value> GetProcessedLength(const Arguments &args);
  static Handle<Value> GetResultLength(const Arguments &args);
  static Handle<Value> GetVisualIndex(const Arguments &args);
  static Handle<Value> GetLogicalIndex(const Arguments &args);

  static Handle<Value> CountParagraphs(const Arguments &args);
  static Handle<Value> GetParagraph(const Arguments &args);
  static Handle<Value> GetParagraphByIndex(const Arguments &args);

  static Handle<Value> CountRuns(const Arguments& args);
  static Handle<Value> GetVisualRun(const Arguments& args);
  static Handle<Value> GetLogicalRun(const Arguments& args);

  static Handle<Value> SetLine(const Arguments& args);
  static Handle<Value> WriteReordered(const Arguments& args);

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
Persistent<FunctionTemplate> Paragraph::constructor_template;

void Paragraph::Init(Handle<Object> target) {
  const char *CLASS_NAME = "Paragraph";
  HandleScope scope;

  Local<FunctionTemplate> t = FunctionTemplate::New(New);

  constructor_template = Persistent<FunctionTemplate>::New(t);
  constructor_template->InstanceTemplate()->SetInternalFieldCount(1);
  constructor_template->SetClassName(String::NewSymbol(CLASS_NAME));

  bidi_SetPrototypeMethod(constructor_template, "countRuns", CountRuns);
  bidi_SetPrototypeMethod(constructor_template, "getVisualRun", GetVisualRun);
  bidi_SetPrototypeMethod(constructor_template, "getLogicalRun", GetLogicalRun);

  bidi_SetPrototypeMethod(constructor_template, "getDirection", GetDirection);
  bidi_SetPrototypeMethod(constructor_template, "getParaLevel", GetParaLevel);
  bidi_SetPrototypeMethod(constructor_template, "getLevelAt", GetLevelAt);
  bidi_SetPrototypeMethod(constructor_template, "getLength", GetLength);
  bidi_SetPrototypeMethod(constructor_template, "getProcessedLength", GetProcessedLength);
  bidi_SetPrototypeMethod(constructor_template, "getResultLength", GetResultLength);

  bidi_SetPrototypeMethod(constructor_template, "getVisualIndex", GetVisualIndex);
  bidi_SetPrototypeMethod(constructor_template, "getLogicalIndex", GetLogicalIndex);

  bidi_SetPrototypeMethod(constructor_template, "countParagraphs", CountParagraphs);
  bidi_SetPrototypeMethod(constructor_template, "getParagraph", GetParagraph);
  bidi_SetPrototypeMethod(constructor_template, "getParagraphByIndex", GetParagraphByIndex);

  bidi_SetPrototypeMethod(constructor_template, "setLine", SetLine);

  bidi_SetPrototypeMethod(constructor_template, "writeReordered", WriteReordered);

  target->Set(String::NewSymbol(CLASS_NAME),
              constructor_template->GetFunction());
}

Handle<Value> Paragraph::New(const Arguments& args) {
  HandleScope scope;
  Local<Value> result;

  if (!args.IsConstructCall()) {
    // Invoked as plain function, turn into construct call
    const int argc = args.Length();
    Local<Value> argv[argc];
    for (int i=0; i<argc; i++) { argv[i] = args[i]; }
    return scope.Close(constructor_template->GetFunction()->NewInstance(argc, argv));
  }
  REQUIRE_ARGUMENTS(1);

  if (args[0]->IsExternal()) {
    // special back door used to create line objects.
    Paragraph *line = (Paragraph *) Local<External>::Cast(args[0])->Value();
    line->Wrap(args.This());
    return scope.Close(args.This());
  }

  String::Value text(args[0]);
  if (*text == NULL) {
    // conversion failed.  toString() threw an exception?
    return ThrowException(Exception::TypeError(String::New(
        "First argument couldn't be converted to a string"
    )));
  }

  // an optional options hash for the second argument.
  Handle<Object> options;
  if (args.Length() <= 1) {
    options = Object::New();
  } else {
    options = args[1]->ToObject();
    if (options.IsEmpty() || !args[1]->IsObject()) {
      return ThrowException(Exception::TypeError(String::New(
        "Second argument should be an options hash"
      )));
    }
  }

  Paragraph *para = new Paragraph();
  para->Wrap(args.This());

  para->para = ubidi_openSized(text.length(), 0, &para->errorCode);
  CHECK_UBIDI_ERR(para);
  if (para->para==NULL) {
    return ThrowException(Exception::Error(String::New("libicu open failed")));
  }

  // parse values from the options hash
  Handle<Value> paraLevelObj =
    options->Get(String::NewSymbol("paraLevel"));
  UBiDiLevel paraLevel =
    paraLevelObj->IsNumber() ? paraLevelObj->Int32Value() :
    UBIDI_DEFAULT_LTR;
  if (!(paraLevel <= UBIDI_MAX_EXPLICIT_LEVEL ||
        paraLevel == UBIDI_DEFAULT_LTR ||
        paraLevel == UBIDI_DEFAULT_RTL)) {
    paraLevel = UBIDI_DEFAULT_LTR;
  }

  Handle<Value> reorderingModeObj =
    options->Get(String::NewSymbol("reorderingMode"));
  UBiDiReorderingMode reorderingMode =
    reorderingModeObj->IsNumber() ?
    (UBiDiReorderingMode) reorderingModeObj->Int32Value() :
    UBIDI_REORDER_COUNT;
  if (reorderingMode >= 0 && reorderingMode < UBIDI_REORDER_COUNT) {
    ubidi_setReorderingMode(para->para, reorderingMode);
  }

  Handle<Value> reorderingOptionsObj =
    options->Get(String::NewSymbol("reorderingOptions"));
  int32_t reorderingOptions =
    reorderingOptionsObj->IsNumber() ? reorderingOptionsObj->Int32Value() :
    -1;
  if (reorderingOptions >= 0 && reorderingOptions <=
      (UBIDI_OPTION_INSERT_MARKS | UBIDI_OPTION_REMOVE_CONTROLS |
       UBIDI_OPTION_STREAMING)) {
    ubidi_setReorderingOptions(para->para, (uint32_t) reorderingOptions);
  }

  Handle<Value> inverseObj =
    options->Get(String::NewSymbol("inverse"));
  if (inverseObj->IsBoolean()) {
    ubidi_setInverse(para->para, inverseObj->BooleanValue());
  }

  Handle<Value> reorderParagraphsLTRObj =
    options->Get(String::NewSymbol("reorderParagraphsLTR"));
  if (reorderParagraphsLTRObj->IsBoolean()) {
    ubidi_orderParagraphsLTR(para->para, reorderParagraphsLTRObj->BooleanValue());
  }

  Handle<Value> prologueObj =
    options->Get(String::NewSymbol("prologue"));
  Handle<Value> epilogueObj =
    options->Get(String::NewSymbol("epilogue"));
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

  return scope.Close(args.This());
}

Handle<Value> Paragraph::SetLine(const Arguments& args) {
  HandleScope scope;
  Paragraph *para = node::ObjectWrap::Unwrap<Paragraph>(args.Holder());
  if (!para->parent.IsEmpty()) {
    return ThrowException(Exception::TypeError(String::New(
      "This is already a line"
    )));
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
    return ThrowException(Exception::Error(String::New("libicu open failed")));
  }

  ubidi_setLine(para->para, start, limit, line->para, &line->errorCode);
  if (U_FAILURE(line->errorCode)) {
    delete line;
    return ThrowException(Exception::Error(String::New("setLine failed")));
  }

  // okay, now create a js object wrapping this Paragraph
  // indicate to the constructor that we just want to wrap this object
  // by passing it as an External
  Local<Value> consArgs[1] = { External::New(line) };
  Local<Object> lineObj = constructor_template->GetFunction()->NewInstance(
    1, consArgs
  );
  line->parent = Persistent<Object>(args.Holder());
  return scope.Close(lineObj);
}

Handle<Value> Paragraph::GetParaLevel(const Arguments& args) {
  HandleScope scope;
  Paragraph *para = node::ObjectWrap::Unwrap<Paragraph>(args.Holder());
  return scope.Close(Integer::New(ubidi_getParaLevel(para->para)));
}

Handle<Value> Paragraph::GetLevelAt(const Arguments& args) {
  HandleScope scope;
  Paragraph *para = node::ObjectWrap::Unwrap<Paragraph>(args.Holder());
  REQUIRE_ARGUMENT_NUMBER(0);
  int32_t charIndex = args[0]->Int32Value();
  return scope.Close(Integer::New(ubidi_getLevelAt(para->para, charIndex)));
}

Handle<Value> Paragraph::CountParagraphs(const Arguments& args) {
  HandleScope scope;
  Paragraph *para = node::ObjectWrap::Unwrap<Paragraph>(args.Holder());
  return scope.Close(Integer::New(ubidi_countParagraphs(para->para)));
}

Handle<Value> Paragraph::GetDirection(const Arguments& args) {
  HandleScope scope;
  Paragraph *para = node::ObjectWrap::Unwrap<Paragraph>(args.Holder());
  UBiDiDirection dir = ubidi_getDirection(para->para);
  return scope.Close(dir2str(dir));
}

Handle<Value> Paragraph::GetLength(const Arguments& args) {
  HandleScope scope;
  Paragraph *para = node::ObjectWrap::Unwrap<Paragraph>(args.Holder());
  return scope.Close(Integer::New(ubidi_getLength(para->para)));
}

Handle<Value> Paragraph::GetProcessedLength(const Arguments& args) {
  HandleScope scope;
  Paragraph *para = node::ObjectWrap::Unwrap<Paragraph>(args.Holder());
  return scope.Close(Integer::New(ubidi_getProcessedLength(para->para)));
}

Handle<Value> Paragraph::GetResultLength(const Arguments& args) {
  HandleScope scope;
  Paragraph *para = node::ObjectWrap::Unwrap<Paragraph>(args.Holder());
  return scope.Close(Integer::New(ubidi_getResultLength(para->para)));
}

Handle<Value> Paragraph::GetVisualIndex(const Arguments& args) {
  HandleScope scope;
  Paragraph *para = node::ObjectWrap::Unwrap<Paragraph>(args.Holder());
  REQUIRE_ARGUMENT_NUMBER(0);
  int32_t logicalIndex = args[0]->Int32Value();
  int32_t visualIndex =
    ubidi_getVisualIndex(para->para, logicalIndex, &para->errorCode);
  CHECK_UBIDI_ERR(para);
  return scope.Close(Integer::New(visualIndex));
}

Handle<Value> Paragraph::GetLogicalIndex(const Arguments& args) {
  HandleScope scope;
  Paragraph *para = node::ObjectWrap::Unwrap<Paragraph>(args.Holder());
  REQUIRE_ARGUMENT_NUMBER(0);
  int32_t visualIndex = args[0]->Int32Value();
  int32_t logicalIndex =
    ubidi_getLogicalIndex(para->para, visualIndex, &para->errorCode);
  CHECK_UBIDI_ERR(para);
  return scope.Close(Integer::New(logicalIndex));
}

Handle<Value> Paragraph::CountRuns(const Arguments& args) {
  HandleScope scope;
  Paragraph *para = node::ObjectWrap::Unwrap<Paragraph>(args.Holder());
  if (para->runs < 0) {
    para->runs = ubidi_countRuns(para->para, &para->errorCode);
    CHECK_UBIDI_ERR(para);
  }
  return scope.Close(Integer::New(para->runs));
}

Handle<Value> Paragraph::GetVisualRun(const Arguments& args) {
  HandleScope scope;
  Paragraph *para = node::ObjectWrap::Unwrap<Paragraph>(args.Holder());
  if (para->runs < 0) {
    para->runs = ubidi_countRuns(para->para, &para->errorCode);
    CHECK_UBIDI_ERR(para);
  }
  REQUIRE_ARGUMENT_NUMBER(0);
  int32_t runIndex = args[0]->Int32Value(), logicalStart, length;
  if (!(runIndex >= 0 && runIndex < para->runs)) {
    return ThrowException(Exception::TypeError(String::New(
      "Run index out of bounds"
    )));
  }
  UBiDiDirection dir = ubidi_getVisualRun(
    para->para, runIndex, &logicalStart, &length
  );
  Local<Object> result = Object::New();
  result->Set(String::NewSymbol("dir"), dir2str(dir));
  result->Set(String::NewSymbol("logicalStart"), Integer::New(logicalStart));
  result->Set(String::NewSymbol("length"), Integer::New(length));
  return scope.Close(result);
}

Handle<Value> Paragraph::GetLogicalRun(const Arguments& args) {
  HandleScope scope;
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
  Local<Object> result = Object::New();
  result->Set(String::NewSymbol("logicalLimit"), Integer::New(logicalLimit));
  result->Set(String::NewSymbol("level"), Integer::New(level));
  result->Set(String::NewSymbol("dir"), dir2str(level2dir(level)));
  return scope.Close(result);
}

Handle<Value> Paragraph::GetParagraph(const Arguments& args) {
  HandleScope scope;
  Paragraph *para = node::ObjectWrap::Unwrap<Paragraph>(args.Holder());
  REQUIRE_ARGUMENT_NUMBER(0);
  int32_t charIndex = args[0]->Int32Value(), paraStart, paraLimit;
  UBiDiLevel paraLevel;
  int32_t paraIndex = ubidi_getParagraph(
    para->para, charIndex, &paraStart, &paraLimit, &paraLevel, &para->errorCode
  );
  CHECK_UBIDI_ERR(para);

  Local<Object> result = Object::New();
  result->Set(String::NewSymbol("index"), Integer::New(paraIndex));
  result->Set(String::NewSymbol("start"), Integer::New(paraStart));
  result->Set(String::NewSymbol("limit"), Integer::New(paraLimit));
  result->Set(String::NewSymbol("level"), Integer::New(paraLevel));
  result->Set(String::NewSymbol("dir"), dir2str(level2dir(paraLevel)));
  return scope.Close(result);
}

Handle<Value> Paragraph::GetParagraphByIndex(const Arguments& args) {
  HandleScope scope;
  Paragraph *para = node::ObjectWrap::Unwrap<Paragraph>(args.Holder());
  REQUIRE_ARGUMENT_NUMBER(0);
  int32_t paraIndex = args[0]->Int32Value(), paraStart, paraLimit;
  UBiDiLevel paraLevel;
  ubidi_getParagraphByIndex(
    para->para, paraIndex, &paraStart, &paraLimit, &paraLevel, &para->errorCode
  );
  CHECK_UBIDI_ERR(para);

  Local<Object> result = Object::New();
  result->Set(String::NewSymbol("index"), Integer::New(paraIndex));
  result->Set(String::NewSymbol("start"), Integer::New(paraStart));
  result->Set(String::NewSymbol("limit"), Integer::New(paraLimit));
  result->Set(String::NewSymbol("level"), Integer::New(paraLevel));
  result->Set(String::NewSymbol("dir"), dir2str(level2dir(paraLevel)));
  return scope.Close(result);
}

Handle<Value> Paragraph::WriteReordered(const Arguments& args) {
  HandleScope scope;
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
    return ThrowException(Exception::Error(String::New(
        "Allocation error (this should never happen)"
    )));
  }
  return scope.Close(String::New(dest, resultSize));
}


// Tell node about our module!
void RegisterModule(Handle<Object> exports) {
  HandleScope scope;

  Paragraph::Init(exports);

  DEFINE_CONSTANT_INTEGER(exports, UBIDI_LTR, LTR);
  DEFINE_CONSTANT_INTEGER(exports, UBIDI_RTL, RTL);
  DEFINE_CONSTANT_INTEGER(exports, UBIDI_DEFAULT_LTR, DEFAULT_LTR);
  DEFINE_CONSTANT_INTEGER(exports, UBIDI_DEFAULT_RTL, DEFAULT_RTL);
  DEFINE_CONSTANT_INTEGER(exports, UBIDI_MAX_EXPLICIT_LEVEL, MAX_EXPLICIT_LEVEL);
  DEFINE_CONSTANT_INTEGER(exports, UBIDI_LEVEL_OVERRIDE, LEVEL_OVERRIDE);
  DEFINE_CONSTANT_INTEGER(exports, UBIDI_MAP_NOWHERE, MAP_NOWHERE);

  // Reordered.<constant>: option bits for writeReordered
  Handle<Object> re = Object::New();
  exports->Set(String::NewSymbol("Reordered"), re,
               static_cast<PropertyAttribute>(ReadOnly | DontDelete));
  DEFINE_CONSTANT_INTEGER(re, UBIDI_KEEP_BASE_COMBINING, KEEP_BASE_COMBINING);
  DEFINE_CONSTANT_INTEGER(re, UBIDI_DO_MIRRORING, DO_MIRRORING);
  DEFINE_CONSTANT_INTEGER(re, UBIDI_INSERT_LRM_FOR_NUMERIC, INSERT_LRM_FOR_NUMERIC);
  DEFINE_CONSTANT_INTEGER(re, UBIDI_REMOVE_BIDI_CONTROLS, REMOVE_BIDI_CONTROLS);
  DEFINE_CONSTANT_INTEGER(re, UBIDI_OUTPUT_REVERSE, OUTPUT_REVERSE);

  // ReorderingMode.<constant>
  Handle<Object> rm = Object::New();
  exports->Set(String::NewSymbol("ReorderingMode"), rm,
               static_cast<PropertyAttribute>(ReadOnly | DontDelete));
  DEFINE_CONSTANT_INTEGER(rm, UBIDI_REORDER_DEFAULT, DEFAULT);
  DEFINE_CONSTANT_INTEGER(rm, UBIDI_REORDER_NUMBERS_SPECIAL, NUMBERS_SPECIAL);
  DEFINE_CONSTANT_INTEGER(rm, UBIDI_REORDER_GROUP_NUMBERS_WITH_R, GROUP_NUMBERS_WITH_R);
  DEFINE_CONSTANT_INTEGER(rm, UBIDI_REORDER_RUNS_ONLY, RUNS_ONLY);
  DEFINE_CONSTANT_INTEGER(rm, UBIDI_REORDER_INVERSE_NUMBERS_AS_L, INVERSE_NUMBERS_AS_L);
  DEFINE_CONSTANT_INTEGER(rm, UBIDI_REORDER_INVERSE_LIKE_DIRECT, INVERSE_LIKE_DIRECT);
  DEFINE_CONSTANT_INTEGER(rm, UBIDI_REORDER_INVERSE_FOR_NUMBERS_SPECIAL, INVERSE_FOR_NUMBERS_SPECIAL);
  //DEFINE_CONSTANT_INTEGER(rm, UBIDI_REORDER_COUNT, COUNT); //number of enums

  Handle<Object> ro = Object::New();
  exports->Set(String::NewSymbol("ReorderingOption"), ro,
               static_cast<PropertyAttribute>(ReadOnly | DontDelete));
  DEFINE_CONSTANT_INTEGER(ro, UBIDI_OPTION_DEFAULT, DEFAULT);
  DEFINE_CONSTANT_INTEGER(ro, UBIDI_OPTION_INSERT_MARKS, INSERT_MARKS);
  DEFINE_CONSTANT_INTEGER(ro, UBIDI_OPTION_REMOVE_CONTROLS, REMOVE_CONTROLS);
  DEFINE_CONSTANT_INTEGER(ro, UBIDI_OPTION_STREAMING, STREAMING);

}

NODE_MODULE(icu_bidi, RegisterModule)

#include <node.h>
#include <v8.h>
#include <cstring> // for std::memcpy

#include "unicode/ubidi.h"

#include "nan.h"
#include "macros.h"

using namespace v8;

/* A type-safe improvement on node::SetPrototypeMethod */
template <typename target_t, typename callback_t>
static void bidi_SetPrototypeMethod(target_t target, const char *name,
                                    callback_t callback) {
  NanScope();
  Local<FunctionTemplate> templ = NanNew<FunctionTemplate>(
    callback, NanUndefined(), NanNew<Signature>(target)
  );
  NanSetPrototypeTemplate(target, name, templ);
}

static Local<Value> bidi_MakeError(UErrorCode code) {
  NanEscapableScope();
  Local<Value> err = Exception::Error(NanNew("The bidi algorithm failed"));
  Local<Object> obj = err.As<Object>();
  obj->Set(NanNew("code"), NanNew<Int32>(code));
  return NanEscapeScope(err);
}

static Handle<Value> dir2str(UBiDiDirection dir) {
  NanEscapableScope();
  return NanEscapeScope(dir == UBIDI_LTR ? NanNew("ltr") :
                        dir == UBIDI_RTL ? NanNew("rtl") :
                        dir == UBIDI_MIXED ? NanNew("mixed") :
                        dir == UBIDI_NEUTRAL ? NanNew("neutral") :
                        NanNew("<bad dir>" /* should never happen */));
}

static UBiDiDirection level2dir(UBiDiLevel level) {
    return (level&1) ? UBIDI_RTL : UBIDI_LTR;
}

#define CHECK_UBIDI_ERR(obj)                                        \
  do {                                                              \
    if (U_FAILURE((obj)->errorCode)) {                              \
      return NanThrowError(bidi_MakeError((obj)->errorCode));       \
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
                errorCode(U_ZERO_ERROR)  {
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
    NanDisposePersistent(parent);
  }

  static NAN_METHOD(New);

  static NAN_METHOD(GetDirection);
  static NAN_METHOD(GetParaLevel);
  static NAN_METHOD(GetLevelAt);
  static NAN_METHOD(GetLength);
  static NAN_METHOD(GetProcessedLength);
  static NAN_METHOD(GetResultLength);
  static NAN_METHOD(GetVisualIndex);
  static NAN_METHOD(GetLogicalIndex);

  static NAN_METHOD(CountParagraphs);
  static NAN_METHOD(GetParagraph);
  static NAN_METHOD(GetParagraphByIndex);

  static NAN_METHOD(CountRuns);
  static NAN_METHOD(GetVisualRun);
  static NAN_METHOD(GetLogicalRun);

  static NAN_METHOD(SetLine);
  static NAN_METHOD(WriteReordered);

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
  NanScope();

  Local<FunctionTemplate> t = NanNew<FunctionTemplate>(New);

  t->InstanceTemplate()->SetInternalFieldCount(1);
  t->SetClassName(NanNew(CLASS_NAME));

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

  NanAssignPersistent(constructor_template, t);
  target->Set(NanNew(CLASS_NAME), t->GetFunction());
}

NAN_METHOD(Paragraph::New) {
  NanScope();
  Local<Value> result;

  if (!args.IsConstructCall()) {
    // Invoked as plain function, turn into construct call
    const int argc = args.Length();
    Local<Value> *argv = new Local<Value>[argc];
    for (int i=0; i<argc; i++) { argv[i] = args[i]; }
    Handle<Value> result = NanNew(constructor_template)->GetFunction()->NewInstance(argc, argv);
    delete[] argv;
    NanReturnValue(result);
  }
  REQUIRE_ARGUMENTS(1);

  if (args[0]->IsExternal()) {
    // special back door used to create line objects.
    Paragraph *line = (Paragraph *) Local<External>::Cast(args[0])->Value();
    line->Wrap(args.This());
    NanReturnValue(args.This());
  }

  String::Value text(args[0]);
  if (*text == NULL) {
    // conversion failed.  toString() threw an exception?
    return NanThrowTypeError(
        "First argument couldn't be converted to a string"
    );
  }

  // an optional options hash for the second argument.
  Handle<Object> options;
  if (args.Length() <= 1) {
    options = NanNew<Object>();
  } else {
    options = args[1]->ToObject();
    if (options.IsEmpty() || !args[1]->IsObject()) {
      return NanThrowTypeError(
        "Second argument should be an options hash"
      );
    }
  }

  Paragraph *para = new Paragraph();
  para->Wrap(args.This());

  para->para = ubidi_openSized(text.length(), 0, &para->errorCode);
  CHECK_UBIDI_ERR(para);
  if (para->para==NULL) {
    return NanThrowError("libicu open failed");
  }

  // parse values from the options hash
  Handle<Value> paraLevelObj =
    options->Get(NanNew("paraLevel"));
  UBiDiLevel paraLevel =
    paraLevelObj->IsNumber() ? paraLevelObj->Int32Value() :
    UBIDI_DEFAULT_LTR;
  if (!(paraLevel <= UBIDI_MAX_EXPLICIT_LEVEL ||
        paraLevel == UBIDI_DEFAULT_LTR ||
        paraLevel == UBIDI_DEFAULT_RTL)) {
    paraLevel = UBIDI_DEFAULT_LTR;
  }

  Handle<Value> reorderingModeObj =
    options->Get(NanNew("reorderingMode"));
  UBiDiReorderingMode reorderingMode =
    reorderingModeObj->IsNumber() ?
    (UBiDiReorderingMode) reorderingModeObj->Int32Value() :
    UBIDI_REORDER_COUNT;
  if (reorderingMode >= 0 && reorderingMode < UBIDI_REORDER_COUNT) {
    ubidi_setReorderingMode(para->para, reorderingMode);
  }

  Handle<Value> reorderingOptionsObj =
    options->Get(NanNew("reorderingOptions"));
  int32_t reorderingOptions =
    reorderingOptionsObj->IsNumber() ? reorderingOptionsObj->Int32Value() :
    -1;
  if (reorderingOptions >= 0 && reorderingOptions <=
      (UBIDI_OPTION_INSERT_MARKS | UBIDI_OPTION_REMOVE_CONTROLS |
       UBIDI_OPTION_STREAMING)) {
    ubidi_setReorderingOptions(para->para, (uint32_t) reorderingOptions);
  }

  Handle<Value> inverseObj =
    options->Get(NanNew("inverse"));
  if (inverseObj->IsBoolean()) {
    ubidi_setInverse(para->para, inverseObj->BooleanValue());
  }

  Handle<Value> reorderParagraphsLTRObj =
    options->Get(NanNew("reorderParagraphsLTR"));
  if (reorderParagraphsLTRObj->IsBoolean()) {
    ubidi_orderParagraphsLTR(para->para, reorderParagraphsLTRObj->BooleanValue());
  }

  Handle<Value> prologueObj =
    options->Get(NanNew("prologue"));
  Handle<Value> epilogueObj =
    options->Get(NanNew("epilogue"));
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

  NanReturnValue(args.This());
}

NAN_METHOD(Paragraph::SetLine) {
  NanScope();
  Paragraph *para = node::ObjectWrap::Unwrap<Paragraph>(args.Holder());
  if (!para->parent.IsEmpty()) {
    return NanThrowTypeError("This is already a line");
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
    return NanThrowError("libicu open failed");
  }

  ubidi_setLine(para->para, start, limit, line->para, &line->errorCode);
  if (U_FAILURE(line->errorCode)) {
    delete line;
    return NanThrowError("setLine failed");
  }

  // okay, now create a js object wrapping this Paragraph
  // indicate to the constructor that we just want to wrap this object
  // by passing it as an External
  Local<Value> consArgs[1] = { NanNew<External>(line) };
  Local<Object> lineObj = NanNew(constructor_template)->GetFunction()->NewInstance(
    1, consArgs
  );
  NanAssignPersistent(line->parent, args.Holder());
  NanReturnValue(lineObj);
}

NAN_METHOD(Paragraph::GetParaLevel) {
  NanScope();
  Paragraph *para = node::ObjectWrap::Unwrap<Paragraph>(args.Holder());
  NanReturnValue(NanNew<Integer>(ubidi_getParaLevel(para->para)));
}

NAN_METHOD(Paragraph::GetLevelAt) {
  NanScope();
  Paragraph *para = node::ObjectWrap::Unwrap<Paragraph>(args.Holder());
  REQUIRE_ARGUMENT_NUMBER(0);
  int32_t charIndex = args[0]->Int32Value();
  NanReturnValue(NanNew<Integer>(ubidi_getLevelAt(para->para, charIndex)));
}

NAN_METHOD(Paragraph::CountParagraphs) {
  NanScope();
  Paragraph *para = node::ObjectWrap::Unwrap<Paragraph>(args.Holder());
  NanReturnValue(NanNew<Integer>(ubidi_countParagraphs(para->para)));
}

NAN_METHOD(Paragraph::GetDirection) {
  NanScope();
  Paragraph *para = node::ObjectWrap::Unwrap<Paragraph>(args.Holder());
  UBiDiDirection dir = ubidi_getDirection(para->para);
  NanReturnValue(dir2str(dir));
}

NAN_METHOD(Paragraph::GetLength) {
  NanScope();
  Paragraph *para = node::ObjectWrap::Unwrap<Paragraph>(args.Holder());
  NanReturnValue(NanNew<Integer>(ubidi_getLength(para->para)));
}

NAN_METHOD(Paragraph::GetProcessedLength) {
  NanScope();
  Paragraph *para = node::ObjectWrap::Unwrap<Paragraph>(args.Holder());
  NanReturnValue(NanNew<Integer>(ubidi_getProcessedLength(para->para)));
}

NAN_METHOD(Paragraph::GetResultLength) {
  NanScope();
  Paragraph *para = node::ObjectWrap::Unwrap<Paragraph>(args.Holder());
  NanReturnValue(NanNew<Integer>(ubidi_getResultLength(para->para)));
}

NAN_METHOD(Paragraph::GetVisualIndex) {
  NanScope();
  Paragraph *para = node::ObjectWrap::Unwrap<Paragraph>(args.Holder());
  REQUIRE_ARGUMENT_NUMBER(0);
  int32_t logicalIndex = args[0]->Int32Value();
  int32_t visualIndex =
    ubidi_getVisualIndex(para->para, logicalIndex, &para->errorCode);
  CHECK_UBIDI_ERR(para);
  NanReturnValue(NanNew<Integer>(visualIndex));
}

NAN_METHOD(Paragraph::GetLogicalIndex) {
  NanScope();
  Paragraph *para = node::ObjectWrap::Unwrap<Paragraph>(args.Holder());
  REQUIRE_ARGUMENT_NUMBER(0);
  int32_t visualIndex = args[0]->Int32Value();
  int32_t logicalIndex =
    ubidi_getLogicalIndex(para->para, visualIndex, &para->errorCode);
  CHECK_UBIDI_ERR(para);
  NanReturnValue(NanNew<Integer>(logicalIndex));
}

NAN_METHOD(Paragraph::CountRuns) {
  NanScope();
  Paragraph *para = node::ObjectWrap::Unwrap<Paragraph>(args.Holder());
  if (para->runs < 0) {
    para->runs = ubidi_countRuns(para->para, &para->errorCode);
    CHECK_UBIDI_ERR(para);
  }
  NanReturnValue(NanNew<Integer>(para->runs));
}

NAN_METHOD(Paragraph::GetVisualRun) {
  NanScope();
  Paragraph *para = node::ObjectWrap::Unwrap<Paragraph>(args.Holder());
  if (para->runs < 0) {
    para->runs = ubidi_countRuns(para->para, &para->errorCode);
    CHECK_UBIDI_ERR(para);
  }
  REQUIRE_ARGUMENT_NUMBER(0);
  int32_t runIndex = args[0]->Int32Value(), logicalStart, length;
  if (!(runIndex >= 0 && runIndex < para->runs)) {
    return NanThrowTypeError("Run index out of bounds");
  }
  UBiDiDirection dir = ubidi_getVisualRun(
    para->para, runIndex, &logicalStart, &length
  );
  Local<Object> result = NanNew<Object>();
  result->Set(NanNew("dir"), dir2str(dir));
  result->Set(NanNew("logicalStart"), NanNew<Integer>(logicalStart));
  result->Set(NanNew("length"), NanNew<Integer>(length));
  NanReturnValue(result);
}

NAN_METHOD(Paragraph::GetLogicalRun) {
  NanScope();
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
  Local<Object> result = NanNew<Object>();
  result->Set(NanNew("logicalLimit"), NanNew<Integer>(logicalLimit));
  result->Set(NanNew("level"), NanNew<Integer>(level));
  result->Set(NanNew("dir"), dir2str(level2dir(level)));
  NanReturnValue(result);
}

NAN_METHOD(Paragraph::GetParagraph) {
  NanScope();
  Paragraph *para = node::ObjectWrap::Unwrap<Paragraph>(args.Holder());
  REQUIRE_ARGUMENT_NUMBER(0);
  int32_t charIndex = args[0]->Int32Value(), paraStart, paraLimit;
  UBiDiLevel paraLevel;
  int32_t paraIndex = ubidi_getParagraph(
    para->para, charIndex, &paraStart, &paraLimit, &paraLevel, &para->errorCode
  );
  CHECK_UBIDI_ERR(para);

  Local<Object> result = NanNew<Object>();
  result->Set(NanNew("index"), NanNew<Integer>(paraIndex));
  result->Set(NanNew("start"), NanNew<Integer>(paraStart));
  result->Set(NanNew("limit"), NanNew<Integer>(paraLimit));
  result->Set(NanNew("level"), NanNew<Integer>(paraLevel));
  result->Set(NanNew("dir"), dir2str(level2dir(paraLevel)));
  NanReturnValue(result);
}

NAN_METHOD(Paragraph::GetParagraphByIndex) {
  NanScope();
  Paragraph *para = node::ObjectWrap::Unwrap<Paragraph>(args.Holder());
  REQUIRE_ARGUMENT_NUMBER(0);
  int32_t paraIndex = args[0]->Int32Value(), paraStart, paraLimit;
  UBiDiLevel paraLevel;
  ubidi_getParagraphByIndex(
    para->para, paraIndex, &paraStart, &paraLimit, &paraLevel, &para->errorCode
  );
  CHECK_UBIDI_ERR(para);

  Local<Object> result = NanNew<Object>();
  result->Set(NanNew("index"), NanNew<Integer>(paraIndex));
  result->Set(NanNew("start"), NanNew<Integer>(paraStart));
  result->Set(NanNew("limit"), NanNew<Integer>(paraLimit));
  result->Set(NanNew("level"), NanNew<Integer>(paraLevel));
  result->Set(NanNew("dir"), dir2str(level2dir(paraLevel)));
  NanReturnValue(result);
}

NAN_METHOD(Paragraph::WriteReordered) {
  NanScope();
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
    return NanThrowError("Allocation error (this should never happen)");
  }
  NanReturnValue(NanNew<String>(dest, resultSize));
}


// Tell node about our module!
void RegisterModule(v8::Handle<Object> exports) {
  NanScope();

  Paragraph::Init(exports);

  DEFINE_CONSTANT_INTEGER(exports, UBIDI_LTR, LTR);
  DEFINE_CONSTANT_INTEGER(exports, UBIDI_RTL, RTL);
  DEFINE_CONSTANT_INTEGER(exports, UBIDI_DEFAULT_LTR, DEFAULT_LTR);
  DEFINE_CONSTANT_INTEGER(exports, UBIDI_DEFAULT_RTL, DEFAULT_RTL);
  DEFINE_CONSTANT_INTEGER(exports, UBIDI_MAX_EXPLICIT_LEVEL, MAX_EXPLICIT_LEVEL);
  DEFINE_CONSTANT_INTEGER(exports, UBIDI_LEVEL_OVERRIDE, LEVEL_OVERRIDE);
  DEFINE_CONSTANT_INTEGER(exports, UBIDI_MAP_NOWHERE, MAP_NOWHERE);

  // Reordered.<constant>: option bits for writeReordered
  Handle<Object> re = NanNew<Object>();
  exports->ForceSet(NanNew("Reordered"), re,
                    static_cast<PropertyAttribute>(ReadOnly | DontDelete));
  DEFINE_CONSTANT_INTEGER(re, UBIDI_KEEP_BASE_COMBINING, KEEP_BASE_COMBINING);
  DEFINE_CONSTANT_INTEGER(re, UBIDI_DO_MIRRORING, DO_MIRRORING);
  DEFINE_CONSTANT_INTEGER(re, UBIDI_INSERT_LRM_FOR_NUMERIC, INSERT_LRM_FOR_NUMERIC);
  DEFINE_CONSTANT_INTEGER(re, UBIDI_REMOVE_BIDI_CONTROLS, REMOVE_BIDI_CONTROLS);
  DEFINE_CONSTANT_INTEGER(re, UBIDI_OUTPUT_REVERSE, OUTPUT_REVERSE);

  // ReorderingMode.<constant>
  Handle<Object> rm = NanNew<Object>();
  exports->ForceSet(NanNew("ReorderingMode"), rm,
                    static_cast<PropertyAttribute>(ReadOnly | DontDelete));
  DEFINE_CONSTANT_INTEGER(rm, UBIDI_REORDER_DEFAULT, DEFAULT);
  DEFINE_CONSTANT_INTEGER(rm, UBIDI_REORDER_NUMBERS_SPECIAL, NUMBERS_SPECIAL);
  DEFINE_CONSTANT_INTEGER(rm, UBIDI_REORDER_GROUP_NUMBERS_WITH_R, GROUP_NUMBERS_WITH_R);
  DEFINE_CONSTANT_INTEGER(rm, UBIDI_REORDER_RUNS_ONLY, RUNS_ONLY);
  DEFINE_CONSTANT_INTEGER(rm, UBIDI_REORDER_INVERSE_NUMBERS_AS_L, INVERSE_NUMBERS_AS_L);
  DEFINE_CONSTANT_INTEGER(rm, UBIDI_REORDER_INVERSE_LIKE_DIRECT, INVERSE_LIKE_DIRECT);
  DEFINE_CONSTANT_INTEGER(rm, UBIDI_REORDER_INVERSE_FOR_NUMBERS_SPECIAL, INVERSE_FOR_NUMBERS_SPECIAL);
  //DEFINE_CONSTANT_INTEGER(rm, UBIDI_REORDER_COUNT, COUNT); //number of enums

  Handle<Object> ro = NanNew<Object>();
  exports->ForceSet(NanNew("ReorderingOption"), ro,
                    static_cast<PropertyAttribute>(ReadOnly | DontDelete));
  DEFINE_CONSTANT_INTEGER(ro, UBIDI_OPTION_DEFAULT, DEFAULT);
  DEFINE_CONSTANT_INTEGER(ro, UBIDI_OPTION_INSERT_MARKS, INSERT_MARKS);
  DEFINE_CONSTANT_INTEGER(ro, UBIDI_OPTION_REMOVE_CONTROLS, REMOVE_CONTROLS);
  DEFINE_CONSTANT_INTEGER(ro, UBIDI_OPTION_STREAMING, STREAMING);

}

NODE_MODULE(icu_bidi, RegisterModule)

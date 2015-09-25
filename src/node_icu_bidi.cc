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
  Nan::HandleScope scope;
  Local<FunctionTemplate> templ = Nan::New<FunctionTemplate>(
    callback, Nan::Undefined(), Nan::New<Signature>(target)
  );
  Nan::SetPrototypeTemplate(target, name, templ);
}

static Local<Value> bidi_MakeError(UErrorCode code) {
  Nan::EscapableHandleScope scope;
  Local<Value> err = Nan::Error("The bidi algorithm failed");
  Local<Object> obj = Nan::To<Object>(err).ToLocalChecked();
  Nan::Set(obj, NEW_STR("code"), Nan::New(code));
  return scope.Escape(err);
}

static Local<Value> dir2str(UBiDiDirection dir) {
  Nan::EscapableHandleScope scope;
  return scope.Escape(dir == UBIDI_LTR ? NEW_STR("ltr") :
                        dir == UBIDI_RTL ? NEW_STR("rtl") :
                        dir == UBIDI_MIXED ? NEW_STR("mixed") :
                        dir == UBIDI_NEUTRAL ? NEW_STR("neutral") :
                        NEW_STR("<bad dir>" /* should never happen */));
}

static UBiDiDirection level2dir(UBiDiLevel level) {
    return (level&1) ? UBIDI_RTL : UBIDI_LTR;
}

#define CHECK_UBIDI_ERR(obj)                                        \
  do {                                                              \
    if (U_FAILURE((obj)->errorCode)) {                              \
      return Nan::ThrowError(bidi_MakeError((obj)->errorCode));     \
    }                                                               \
  } while (false)

// declarations
class Paragraph : public Nan::ObjectWrap {
public:
  static Nan::Persistent<FunctionTemplate> constructor_template;
  static void Init(Nan::ADDON_REGISTER_FUNCTION_ARGS_TYPE target);
protected:
  Paragraph() : Nan::ObjectWrap(),
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
    parent.Reset();
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
  Nan::Persistent<Object> parent;
};

// implementation
Nan::Persistent<FunctionTemplate> Paragraph::constructor_template;

void Paragraph::Init(Nan::ADDON_REGISTER_FUNCTION_ARGS_TYPE target) {
  const char *CLASS_NAME = "Paragraph";
  Nan::HandleScope scope;

  Local<FunctionTemplate> t = Nan::New<FunctionTemplate>(New);

  t->InstanceTemplate()->SetInternalFieldCount(1);
  t->SetClassName(NEW_STR(CLASS_NAME));

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

  constructor_template.Reset(t);
  Nan::Set(target, NEW_STR(CLASS_NAME), Nan::GetFunction(t).ToLocalChecked());
}

NAN_METHOD(Paragraph::New) {
  Local<Value> result;

  if (!info.IsConstructCall()) {
    // Invoked as plain function, turn into construct call
    const int argc = info.Length();
    Local<Value> *argv = new Local<Value>[argc];
    for (int i=0; i<argc; i++) { argv[i] = info[i]; }
    Nan::TryCatch try_catch;
    Nan::MaybeLocal<Object> result = Nan::NewInstance(
      Nan::GetFunction(Nan::New<FunctionTemplate>(constructor_template))
      .ToLocalChecked(), argc, argv);
    delete[] argv;
    if (try_catch.HasCaught()) {
      try_catch.ReThrow();
    } else {
      info.GetReturnValue().Set(result.ToLocalChecked());
    }
    return;
  }
  REQUIRE_ARGUMENTS(1);

  if (info[0]->IsExternal()) {
    // special back door used to create line objects.
    Paragraph *line = (Paragraph *) Local<External>::Cast(info[0])->Value();
    line->Wrap(info.This());
    info.GetReturnValue().Set(info.This());
    return;
  }

  Nan::MaybeLocal<String> maybeText = Nan::To<String>(info[0]);
  if (maybeText.IsEmpty()) {
    // conversion failed.  toString() threw an exception?
    return Nan::ThrowTypeError(
        "First argument couldn't be converted to a string"
    );
  }
  Local<String> text = maybeText.ToLocalChecked();
  int32_t tlen = text->Length();

  // an optional options hash for the second argument.
  Local<Object> options;
  if (info.Length() <= 1) {
    options = Nan::New<Object>();
  } else {
    Nan::MaybeLocal<Object> maybeOptions = Nan::To<Object>(info[1]);
    if (maybeOptions.IsEmpty()) {
      return Nan::ThrowTypeError(
        "Second argument should be an options hash"
      );
    }
    options = maybeOptions.ToLocalChecked();
  }

  Paragraph *para = new Paragraph();
  para->Wrap(info.This());

  para->para = ubidi_openSized(tlen, 0, &para->errorCode);
  CHECK_UBIDI_ERR(para);
  if (para->para==NULL) {
    return Nan::ThrowError("libicu open failed");
  }

  // parse values from the options hash
  UBiDiLevel paraLevel = (UBiDiLevel)
    CAST_INT(GET_PROPERTY(options, "paraLevel"), UBIDI_DEFAULT_LTR);
  if (!(paraLevel <= UBIDI_MAX_EXPLICIT_LEVEL ||
        paraLevel == UBIDI_DEFAULT_LTR ||
        paraLevel == UBIDI_DEFAULT_RTL)) {
    paraLevel = UBIDI_DEFAULT_LTR;
  }

  UBiDiReorderingMode reorderingMode = (UBiDiReorderingMode)
    CAST_INT(GET_PROPERTY(options, "reorderingMode"), UBIDI_REORDER_COUNT);
  if (reorderingMode >= 0 && reorderingMode < UBIDI_REORDER_COUNT) {
    ubidi_setReorderingMode(para->para, reorderingMode);
  }

  int32_t reorderingOptions =
    CAST_INT(GET_PROPERTY(options, "reorderingOptions"), -1);
  if (reorderingOptions >= 0 && reorderingOptions <=
      (UBIDI_OPTION_INSERT_MARKS | UBIDI_OPTION_REMOVE_CONTROLS |
       UBIDI_OPTION_STREAMING)) {
    ubidi_setReorderingOptions(para->para, (uint32_t) reorderingOptions);
  }

  Local<Value> inverse =
    GET_PROPERTY(options, "inverse");
  if (inverse->IsBoolean()) {
    ubidi_setInverse(para->para, CAST_BOOL(inverse, false));
  }

  Local<Value> reorderParagraphsLTR =
    GET_PROPERTY(options, "reorderParagraphsLTR");
  if (reorderParagraphsLTR->IsBoolean()) {
    ubidi_orderParagraphsLTR(para->para, CAST_BOOL(reorderParagraphsLTR, false));
  }

  Local<String> prologue =
    CAST_STRING(GET_PROPERTY(options, "prologue"), Nan::EmptyString());
  Local<String> epilogue =
    CAST_STRING(GET_PROPERTY(options, "epilogue"), Nan::EmptyString());
  int32_t plen = prologue->Length();
  int32_t elen = epilogue->Length();

  // Copy main, prologue, and epilogue text and keep it alive as long
  // as we're alive.
  para->text = new UChar[plen + tlen + elen];
  prologue->Write(para->text, 0, plen);
  text->Write(para->text + plen, 0, tlen);
  epilogue->Write(para->text + plen + tlen, 0, elen);
  if (plen!=0 || elen!=0) {
    ubidi_setContext(
      para->para, para->text, plen,
      para->text + plen + tlen, elen,
      &para->errorCode
    );
    CHECK_UBIDI_ERR(para);
  }

  // XXX parse options.embeddingLevels and construct an appropriate array of
  //     UBiDiLevel to pass to setPara

  ubidi_setPara(para->para, para->text + plen, tlen,
                paraLevel, NULL, &para->errorCode);
  CHECK_UBIDI_ERR(para);

  info.GetReturnValue().Set(info.This());
}

NAN_METHOD(Paragraph::SetLine) {
  Paragraph *para = Nan::ObjectWrap::Unwrap<Paragraph>(info.Holder());
  if (!para->parent.IsEmpty()) {
    return Nan::ThrowTypeError("This is already a line");
  }
  REQUIRE_ARGUMENT_NUMBER(0);
  REQUIRE_ARGUMENT_NUMBER(1);
  int32_t start = Nan::To<int32_t>(info[0]).FromJust();
  int32_t limit = Nan::To<int32_t>(info[1]).FromJust();

  // Create para object.
  Paragraph *line = new Paragraph();
  line->para = ubidi_openSized(ubidi_getLength(para->para), 0, &line->errorCode);
  if (U_FAILURE(line->errorCode) || line->para==NULL) {
    delete line;
    return Nan::ThrowError("libicu open failed");
  }

  ubidi_setLine(para->para, start, limit, line->para, &line->errorCode);
  if (U_FAILURE(line->errorCode)) {
    delete line;
    return Nan::ThrowError("setLine failed");
  }

  // okay, now create a js object wrapping this Paragraph
  // indicate to the constructor that we just want to wrap this object
  // by passing it as an External
  Local<Value> consArgs[1] = { Nan::New<External>(line) };
  Local<Object> lineObj = Nan::NewInstance(
    Nan::GetFunction(Nan::New(constructor_template)).ToLocalChecked(),
    1, consArgs).ToLocalChecked();
  line->parent.Reset(info.Holder());
  info.GetReturnValue().Set(lineObj);
}

NAN_METHOD(Paragraph::GetParaLevel) {
  Paragraph *para = Nan::ObjectWrap::Unwrap<Paragraph>(info.Holder());
  info.GetReturnValue().Set(Nan::New(ubidi_getParaLevel(para->para)));
}

NAN_METHOD(Paragraph::GetLevelAt) {
  Paragraph *para = Nan::ObjectWrap::Unwrap<Paragraph>(info.Holder());
  REQUIRE_ARGUMENT_NUMBER(0);
  int32_t charIndex = Nan::To<int32_t>(info[0]).FromJust();
  info.GetReturnValue().Set(Nan::New(ubidi_getLevelAt(para->para, charIndex)));
}

NAN_METHOD(Paragraph::CountParagraphs) {
  Paragraph *para = Nan::ObjectWrap::Unwrap<Paragraph>(info.Holder());
  info.GetReturnValue().Set(Nan::New(ubidi_countParagraphs(para->para)));
}

NAN_METHOD(Paragraph::GetDirection) {
  Paragraph *para = Nan::ObjectWrap::Unwrap<Paragraph>(info.Holder());
  UBiDiDirection dir = ubidi_getDirection(para->para);
  info.GetReturnValue().Set(dir2str(dir));
}

NAN_METHOD(Paragraph::GetLength) {
  Paragraph *para = Nan::ObjectWrap::Unwrap<Paragraph>(info.Holder());
  info.GetReturnValue().Set(Nan::New(ubidi_getLength(para->para)));
}

NAN_METHOD(Paragraph::GetProcessedLength) {
  Paragraph *para = Nan::ObjectWrap::Unwrap<Paragraph>(info.Holder());
  info.GetReturnValue().Set(Nan::New(ubidi_getProcessedLength(para->para)));
}

NAN_METHOD(Paragraph::GetResultLength) {
  Paragraph *para = Nan::ObjectWrap::Unwrap<Paragraph>(info.Holder());
  info.GetReturnValue().Set(Nan::New(ubidi_getResultLength(para->para)));
}

NAN_METHOD(Paragraph::GetVisualIndex) {
  Paragraph *para = Nan::ObjectWrap::Unwrap<Paragraph>(info.Holder());
  REQUIRE_ARGUMENT_NUMBER(0);
  int32_t logicalIndex = Nan::To<int32_t>(info[0]).FromJust();
  int32_t visualIndex =
    ubidi_getVisualIndex(para->para, logicalIndex, &para->errorCode);
  CHECK_UBIDI_ERR(para);
  info.GetReturnValue().Set(Nan::New(visualIndex));
}

NAN_METHOD(Paragraph::GetLogicalIndex) {
  Paragraph *para = Nan::ObjectWrap::Unwrap<Paragraph>(info.Holder());
  REQUIRE_ARGUMENT_NUMBER(0);
  int32_t visualIndex = Nan::To<int32_t>(info[0]).FromJust();
  int32_t logicalIndex =
    ubidi_getLogicalIndex(para->para, visualIndex, &para->errorCode);
  CHECK_UBIDI_ERR(para);
  info.GetReturnValue().Set(Nan::New(logicalIndex));
}

NAN_METHOD(Paragraph::CountRuns) {
  Paragraph *para = Nan::ObjectWrap::Unwrap<Paragraph>(info.Holder());
  if (para->runs < 0) {
    para->runs = ubidi_countRuns(para->para, &para->errorCode);
    CHECK_UBIDI_ERR(para);
  }
  info.GetReturnValue().Set(Nan::New(para->runs));
}

NAN_METHOD(Paragraph::GetVisualRun) {
  Paragraph *para = Nan::ObjectWrap::Unwrap<Paragraph>(info.Holder());
  if (para->runs < 0) {
    para->runs = ubidi_countRuns(para->para, &para->errorCode);
    CHECK_UBIDI_ERR(para);
  }
  REQUIRE_ARGUMENT_NUMBER(0);
  int32_t runIndex = Nan::To<int32_t>(info[0]).FromJust(), logicalStart, length;
  if (!(runIndex >= 0 && runIndex < para->runs)) {
    return Nan::ThrowTypeError("Run index out of bounds");
  }
  UBiDiDirection dir = ubidi_getVisualRun(
    para->para, runIndex, &logicalStart, &length
  );
  Local<Object> result = Nan::New<Object>();
  Nan::Set(result, NEW_STR("dir"), dir2str(dir));
  Nan::Set(result, NEW_STR("logicalStart"), Nan::New(logicalStart));
  Nan::Set(result, NEW_STR("length"), Nan::New(length));
  info.GetReturnValue().Set(result);
}

NAN_METHOD(Paragraph::GetLogicalRun) {
  Paragraph *para = Nan::ObjectWrap::Unwrap<Paragraph>(info.Holder());
  if (para->runs < 0) {
    para->runs = ubidi_countRuns(para->para, &para->errorCode);
    CHECK_UBIDI_ERR(para);
  }
  REQUIRE_ARGUMENT_NUMBER(0);
  int32_t logicalPosition = Nan::To<int32_t>(info[0]).FromJust(), logicalLimit;
  UBiDiLevel level;
  ubidi_getLogicalRun(
    para->para, logicalPosition, &logicalLimit, &level
  );
  Local<Object> result = Nan::New<Object>();
  Nan::Set(result, NEW_STR("logicalLimit"), Nan::New(logicalLimit));
  Nan::Set(result, NEW_STR("level"), Nan::New(level));
  Nan::Set(result, NEW_STR("dir"), dir2str(level2dir(level)));
  info.GetReturnValue().Set(result);
}

NAN_METHOD(Paragraph::GetParagraph) {
  Paragraph *para = Nan::ObjectWrap::Unwrap<Paragraph>(info.Holder());
  REQUIRE_ARGUMENT_NUMBER(0);
  int32_t charIndex = Nan::To<int32_t>(info[0]).FromJust(), paraStart, paraLimit;
  UBiDiLevel paraLevel;
  int32_t paraIndex = ubidi_getParagraph(
    para->para, charIndex, &paraStart, &paraLimit, &paraLevel, &para->errorCode
  );
  CHECK_UBIDI_ERR(para);

  Local<Object> result = Nan::New<Object>();
  Nan::Set(result, NEW_STR("index"), Nan::New(paraIndex));
  Nan::Set(result, NEW_STR("start"), Nan::New(paraStart));
  Nan::Set(result, NEW_STR("limit"), Nan::New(paraLimit));
  Nan::Set(result, NEW_STR("level"), Nan::New(paraLevel));
  Nan::Set(result, NEW_STR("dir"), dir2str(level2dir(paraLevel)));
  info.GetReturnValue().Set(result);
}

NAN_METHOD(Paragraph::GetParagraphByIndex) {
  Paragraph *para = Nan::ObjectWrap::Unwrap<Paragraph>(info.Holder());
  REQUIRE_ARGUMENT_NUMBER(0);
  int32_t paraIndex = Nan::To<int32_t>(info[0]).FromJust(), paraStart, paraLimit;
  UBiDiLevel paraLevel;
  ubidi_getParagraphByIndex(
    para->para, paraIndex, &paraStart, &paraLimit, &paraLevel, &para->errorCode
  );
  CHECK_UBIDI_ERR(para);

  Local<Object> result = Nan::New<Object>();
  Nan::Set(result, NEW_STR("index"), Nan::New(paraIndex));
  Nan::Set(result, NEW_STR("start"), Nan::New(paraStart));
  Nan::Set(result, NEW_STR("limit"), Nan::New(paraLimit));
  Nan::Set(result, NEW_STR("level"), Nan::New(paraLevel));
  Nan::Set(result, NEW_STR("dir"), dir2str(level2dir(paraLevel)));
  info.GetReturnValue().Set(result);
}

NAN_METHOD(Paragraph::WriteReordered) {
  Paragraph *para = Nan::ObjectWrap::Unwrap<Paragraph>(info.Holder());
  uint16_t options = 0;
  if (info.Length() > 0) {
    REQUIRE_ARGUMENT_NUMBER(0);
    options = (uint16_t) Nan::To<uint32_t>(info[0]).FromJust();
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
    return Nan::ThrowError("Allocation error (this should never happen)");
  }
  info.GetReturnValue().Set(Nan::New<String>(dest, resultSize).ToLocalChecked());
}


// Tell node about our module!
NAN_MODULE_INIT(RegisterModule) {
  Nan::HandleScope scope;

  Paragraph::Init(target);

  DEFINE_CONSTANT_INTEGER(target, UBIDI_LTR, LTR);
  DEFINE_CONSTANT_INTEGER(target, UBIDI_RTL, RTL);
  DEFINE_CONSTANT_INTEGER(target, UBIDI_DEFAULT_LTR, DEFAULT_LTR);
  DEFINE_CONSTANT_INTEGER(target, UBIDI_DEFAULT_RTL, DEFAULT_RTL);
  DEFINE_CONSTANT_INTEGER(target, UBIDI_MAX_EXPLICIT_LEVEL, MAX_EXPLICIT_LEVEL);
  DEFINE_CONSTANT_INTEGER(target, UBIDI_LEVEL_OVERRIDE, LEVEL_OVERRIDE);
  DEFINE_CONSTANT_INTEGER(target, UBIDI_MAP_NOWHERE, MAP_NOWHERE);

  // Reordered.<constant>: option bits for writeReordered
  Local<Object> re = Nan::New<Object>();
  Nan::ForceSet(target, NEW_STR("Reordered"), re,
                    static_cast<PropertyAttribute>(ReadOnly | DontDelete));
  DEFINE_CONSTANT_INTEGER(re, UBIDI_KEEP_BASE_COMBINING, KEEP_BASE_COMBINING);
  DEFINE_CONSTANT_INTEGER(re, UBIDI_DO_MIRRORING, DO_MIRRORING);
  DEFINE_CONSTANT_INTEGER(re, UBIDI_INSERT_LRM_FOR_NUMERIC, INSERT_LRM_FOR_NUMERIC);
  DEFINE_CONSTANT_INTEGER(re, UBIDI_REMOVE_BIDI_CONTROLS, REMOVE_BIDI_CONTROLS);
  DEFINE_CONSTANT_INTEGER(re, UBIDI_OUTPUT_REVERSE, OUTPUT_REVERSE);

  // ReorderingMode.<constant>
  Local<Object> rm = Nan::New<Object>();
  Nan::ForceSet(target, NEW_STR("ReorderingMode"), rm,
                    static_cast<PropertyAttribute>(ReadOnly | DontDelete));
  DEFINE_CONSTANT_INTEGER(rm, UBIDI_REORDER_DEFAULT, DEFAULT);
  DEFINE_CONSTANT_INTEGER(rm, UBIDI_REORDER_NUMBERS_SPECIAL, NUMBERS_SPECIAL);
  DEFINE_CONSTANT_INTEGER(rm, UBIDI_REORDER_GROUP_NUMBERS_WITH_R, GROUP_NUMBERS_WITH_R);
  DEFINE_CONSTANT_INTEGER(rm, UBIDI_REORDER_RUNS_ONLY, RUNS_ONLY);
  DEFINE_CONSTANT_INTEGER(rm, UBIDI_REORDER_INVERSE_NUMBERS_AS_L, INVERSE_NUMBERS_AS_L);
  DEFINE_CONSTANT_INTEGER(rm, UBIDI_REORDER_INVERSE_LIKE_DIRECT, INVERSE_LIKE_DIRECT);
  DEFINE_CONSTANT_INTEGER(rm, UBIDI_REORDER_INVERSE_FOR_NUMBERS_SPECIAL, INVERSE_FOR_NUMBERS_SPECIAL);
  //DEFINE_CONSTANT_INTEGER(rm, UBIDI_REORDER_COUNT, COUNT); //number of enums

  Local<Object> ro = Nan::New<Object>();
  Nan::ForceSet(target, NEW_STR("ReorderingOption"), ro,
                    static_cast<PropertyAttribute>(ReadOnly | DontDelete));
  DEFINE_CONSTANT_INTEGER(ro, UBIDI_OPTION_DEFAULT, DEFAULT);
  DEFINE_CONSTANT_INTEGER(ro, UBIDI_OPTION_INSERT_MARKS, INSERT_MARKS);
  DEFINE_CONSTANT_INTEGER(ro, UBIDI_OPTION_REMOVE_CONTROLS, REMOVE_CONTROLS);
  DEFINE_CONSTANT_INTEGER(ro, UBIDI_OPTION_STREAMING, STREAMING);

}

NODE_MODULE(icu_bidi, RegisterModule)

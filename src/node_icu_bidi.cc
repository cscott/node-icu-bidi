#include <node.h>
#include <v8.h>

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
                runs(-1),
                errorCode(U_ZERO_ERROR) {
  }
  ~Paragraph() {
    if (para != NULL) {
      ubidi_close(para);
      para = NULL;
    }
  }

  static Handle<Value> New(const Arguments& args);
  static Handle<Value> GetDirection(const Arguments &args);
  static Handle<Value> GetParaLevel(const Arguments &args);
  static Handle<Value> CountRuns(const Arguments& args);
  static Handle<Value> GetVisualRun(const Arguments& args);

protected:
  UBiDi *para;
  int32_t runs;
  UErrorCode errorCode;
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

  bidi_SetPrototypeMethod(constructor_template, "getDirection", GetDirection);
  bidi_SetPrototypeMethod(constructor_template, "getParaLevel", GetParaLevel);

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

  String::Value text(args[0]);
  if (*text == NULL) {
    // conversion failed.  toString() threw an exception?
    return ThrowException(Exception::TypeError(String::New(
        "First argument couldn't be converted to a string"
    )));
  }
  bool direction = (args.Length() <= 1) ? false : args[1]->BooleanValue();

  Paragraph *para = new Paragraph();
  para->Wrap(args.This());

  para->para = ubidi_openSized(text.length(), 0, &para->errorCode);
  if (para->para==NULL) {
    return ThrowException(Exception::Error(String::New("libicu open failed")));
  }

  ubidi_setPara(para->para, *text, text.length(),
                direction ? UBIDI_DEFAULT_RTL : UBIDI_DEFAULT_LTR,
                NULL, &para->errorCode);
  CHECK_UBIDI_ERR(para);

  return args.This();
}

Handle<Value> Paragraph::GetParaLevel(const Arguments& args) {
  HandleScope scope;
  Paragraph *para = node::ObjectWrap::Unwrap<Paragraph>(args.Holder());
  return scope.Close(Integer::New(ubidi_getParaLevel(para->para)));
}

Handle<Value> Paragraph::GetDirection(const Arguments& args) {
  HandleScope scope;
  Paragraph *para = node::ObjectWrap::Unwrap<Paragraph>(args.Holder());
  UBiDiDirection dir = ubidi_getDirection(para->para);
  return scope.Close(dir2str(dir));
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


// Tell node about our module!
void RegisterModule(Handle<Object> exports) {
  HandleScope scope;

  Paragraph::Init(exports);
}

NODE_MODULE(icu_bidi, RegisterModule)

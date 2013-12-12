#include <node.h>
#include <v8.h>

#include "unicode/ubidi.h"

#include "macros.h"

using namespace v8;

/* A type-safe improvement on node::SetPrototypeMethod */
template <typename target_t>
void bidi_SetPrototypeMethod(target_t target, const char *name,
                             v8::InvocationCallback callback) {
  Local<FunctionTemplate> templ = FunctionTemplate::New(
    callback, Undefined(), Signature::New(target)
  );
  target->PrototypeTemplate()->Set(String::NewSymbol(name), templ);
}

// declarations
class Paragraph : public node::ObjectWrap {
public:
  static Persistent<FunctionTemplate> constructor_template;
  static void Init(Handle<Object> target);
protected:
  Paragraph() : node::ObjectWrap(),
                para(NULL),
                runs(-1) {
  }
  ~Paragraph() {
    if (para != NULL) {
      ubidi_close(para);
      para = NULL;
    }
  }

  static Handle<Value> New(const Arguments& args);
  static Handle<Value> CountRuns(const Arguments& args);
  static Handle<Value> GetVisualRun(const Arguments& args);

protected:
  UBiDi *para;
  int runs;
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

  target->Set(String::NewSymbol(CLASS_NAME),
              constructor_template->GetFunction());
}

Handle<Value> Paragraph::New(const Arguments& args) {
  HandleScope scope;
  Local<Value> result;
  UErrorCode errorCode = U_ZERO_ERROR;

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

  para->para = ubidi_openSized(text.length(), 0, &errorCode);
  if (para->para==NULL) {
    return ThrowException(Exception::Error(String::New("libicu open failed")));
  }

  ubidi_setPara(para->para, *text, text.length(),
                direction ? UBIDI_DEFAULT_RTL : UBIDI_DEFAULT_LTR,
                NULL, &errorCode);
  if (!U_SUCCESS(errorCode)) {
    // xxx stash away the 'errorCode' somewhere?
    return ThrowException(Exception::TypeError(String::New(
      "The bidi algorithm failed"
    )));
  }

  return args.This();
}

Handle<Value> Paragraph::CountRuns(const Arguments& args) {
  HandleScope scope;
  Paragraph *para = node::ObjectWrap::Unwrap<Paragraph>(args.Holder());
  return Undefined();
}

Handle<Value> Paragraph::GetVisualRun(const Arguments& args) {
  HandleScope scope;
  Paragraph *para = node::ObjectWrap::Unwrap<Paragraph>(args.Holder());
  return Undefined();
}


// Tell node about our module!
void RegisterModule(Handle<Object> exports) {
  HandleScope scope;

  Paragraph::Init(exports);
}

NODE_MODULE(icu_bidi, RegisterModule)

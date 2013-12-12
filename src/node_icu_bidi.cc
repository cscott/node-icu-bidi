#include <node.h>
#include <v8.h>

#include "unicode/ubidi.h"

#include "macros.h"

using namespace v8;

Handle<Value> hello_method(const Arguments& args) {
  HandleScope scope;
  return scope.Close(String::New("world"));
}

Handle<Value> analyze_method(const Arguments& args) {
  HandleScope scope;
  Local<Value> result;
  UErrorCode errorCode = U_ZERO_ERROR;

  REQUIRE_ARGUMENTS(1);

  String::Value text(args[0]);
  if (*text == NULL) {
    // conversion failed.  toString() threw an exception?
    return ThrowException(Exception::TypeError(String::New(
        "first argument couldn't be converted to a string"
    )));
  }
  bool direction = (args.Length() <= 1) ? false : args[1]->BooleanValue();

  UBiDi *para = ubidi_openSized(text.length(), 0, &errorCode);
  if (para==NULL) {
    return ThrowException(Exception::Error(String::New("libicu open failed")));
  }

  result = Integer::New(text.length());
  ubidi_setPara(para, *text, text.length(),
                direction ? UBIDI_DEFAULT_RTL : UBIDI_DEFAULT_LTR,
                NULL, &errorCode);
  if (U_SUCCESS(errorCode)) {
    // yay!
    result = Integer::New(-1);
  }
  ubidi_close(para);

  return scope.Close(result);
}

void init(Handle<Object> exports) {
  HandleScope scope;
  exports->Set(String::NewSymbol("hello"),
               FunctionTemplate::New(hello_method)->GetFunction());
  exports->Set(String::NewSymbol("analyze"),
               FunctionTemplate::New(analyze_method)->GetFunction());
}

NODE_MODULE(icu_bidi, init)

#ifndef NODE_ICU_BIDI_SRC_MACROS_H
#define NODE_ICU_BIDI_SRC_MACROS_H


#define REQUIRE_ARGUMENTS(n)                                                   \
    if (args.Length() < (n)) {                                                 \
        SET_FUNCTION_RETURN_VALUE_AND_RETURN(THROW_EXCEPTION("Expected " #n " arguments"));     \
    }


#define REQUIRE_ARGUMENT_EXTERNAL(i, var)                                      \
    if (args.Length() <= (i) || !args[i]->IsExternal()) {                      \
        SET_FUNCTION_RETURN_VALUE_AND_RETURN(THROW_EXCEPTION("Argument " #i " invalid"));      \
    }                                                                          \
    Local<External> var = Local<External>::Cast(args[i]);


#define REQUIRE_ARGUMENT_FUNCTION(i, var)                                      \
    if (args.Length() <= (i) || !args[i]->IsFunction()) {                      \
        SET_FUNCTION_RETURN_VALUE_AND_RETURN(THROW_EXCEPTION("Argument " #i " must be a function"));                \
    }                                                                          \
    Local<Function> var = Local<Function>::Cast(args[i]);


#define REQUIRE_ARGUMENT_STRING(i, var)                                        \
    if (args.Length() <= (i) || !args[i]->IsString()) {                        \
        SET_FUNCTION_RETURN_VALUE_AND_RETURN(THROW_EXCEPTION("Argument " #i " must be a string"));                   \
    }                                                                          \
    String::Utf8Value var(args[i]->ToString());

#define REQUIRE_ARGUMENT_NUMBER(i)                                      \
    if (args.Length() <= (i) || !args[i]->IsNumber()) {                 \
        SET_FUNCTION_RETURN_VALUE_AND_RETURN(THROW_EXCEPTION("Argument " #i " must be a number"));             \
    }

#define REQUIRE_ARGUMENT_INTEGER(i, var)                                       \
    if (args.Length() <= (i) || !(args[i]->IsInt32() || args[i]->IsUint32())) { \
        SET_FUNCTION_RETURN_VALUE_AND_RETURN(THROW_EXCEPTION("Argument " #i " must be an integer"));                \
    }                                                                          \
    int64_t var = args[i]->IntegerValue();


#define OPTIONAL_ARGUMENT_FUNCTION(i, var)                                     \
    Local<Function> var;                                                       \
    if (args.Length() > i && !args[i]->IsUndefined()) {                        \
        if (!args[i]->IsFunction()) {                                          \
            THROW_EXCEPTION("Argument " #i " must be a function");             \        }                                                                      \
        var = Local<Function>::Cast(args[i]);                                  \
    }


#define OPTIONAL_ARGUMENT_BOOLEAN(i, var, default)                             \
    int var;                                                                   \
    if (args.Length() <= (i)) {                                                \
        var = (default);                                                       \
    }                                                                          \
    else {                                                                     \
        var = args[i]->ToBoolean();                                            \
    }

#define OPTIONAL_ARGUMENT_INTEGER(i, var, default)                             \
    int var;                                                                   \
    if (args.Length() <= (i)) {                                                \
        var = (default);                                                       \
    }                                                                          \
    else if (args[i]->IsInt32()) {                                             \
        var = args[i]->Int32Value();                                           \
    }                                                                          \
    else {                                                                     \
        return THROW_EXCEPTION("Argument " #i " must be an integer");          \
    }


#define DEFINE_CONSTANT_INTEGER(target, constant, name)                        \
    (target)->SET_WITH_ATTRIBUTES_METHOD(                                                             \
        NEW_SYMBOL(#name),                                              \
        NEW_INTEGER(constant),                                                \
        static_cast<PropertyAttribute>(ReadOnly | DontDelete)                  \
    );

#define DEFINE_CONSTANT_STRING(target, constant, name)                         \
    (target)->SET_WITH_ATTRIBUTES_METHOD(                                                             \
        NEW_SYMBOL(#name),                                              \
        NEW_SYMBOL(constant),                                           \
        static_cast<PropertyAttribute>(ReadOnly | DontDelete)                  \
    );


#define NODE_SET_GETTER(target, name, function)                                \
    (target)->InstanceTemplate()                                               \
        ->SetAccessor(NEW_SYMBOL(name), (function));

#define GET_STRING(source, name, property)                                     \
    String::Utf8Value name((source)->Get(NEW_SYMBOL(property)));

#define GET_INTEGER(source, name, property)                                    \
    int name = (source)->Get(NEW_SYMBOL(property))->Int32Value();

#define EMIT_EVENT(obj, argc, argv)                                            \
    TRY_CATCH_CALL((obj),                                                      \
        Local<Function>::Cast((obj)->Get(NEW_SYMBOL("emit"))),          \
        argc, argv                                                             \
    );

#define TRY_CATCH_CALL(context, callback, argc, argv)                          \
{   TryCatch try_catch;                                                        \
    (callback)->Call((context), (argc), (argv));                               \
    if (try_catch.HasCaught()) {                                               \
        FatalException(try_catch);                                             \
    }                                                                          }

#endif


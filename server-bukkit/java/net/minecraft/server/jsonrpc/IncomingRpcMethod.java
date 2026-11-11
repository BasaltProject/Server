// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.jsonrpc;

import com.google.gson.JsonArray;
import com.google.gson.JsonElement;
import com.google.gson.JsonObject;
import com.mojang.serialization.JsonOps;
import java.util.function.Function;
import net.minecraft.core.Registry;
import net.minecraft.resources.Identifier;
import net.minecraft.server.jsonrpc.api.MethodInfo;
import net.minecraft.server.jsonrpc.api.ParamInfo;
import net.minecraft.server.jsonrpc.api.ResultInfo;
import net.minecraft.server.jsonrpc.api.Schema;
import net.minecraft.server.jsonrpc.internalapi.MinecraftApi;
import net.minecraft.server.jsonrpc.methods.ClientInfo;
import net.minecraft.server.jsonrpc.methods.EncodeJsonRpcException;
import net.minecraft.server.jsonrpc.methods.InvalidParameterJsonRpcException;

public interface IncomingRpcMethod<Params, Result> {
    MethodInfo<Params, Result> info();

    Attributes attributes();

    JsonElement apply(MinecraftApi minecraftApi, JsonElement paramsJson, ClientInfo clientInfo);

    private static <R> JsonElement encodeResult(MethodInfo<?, R> info, R result, boolean badParams) {
        ResultInfo<R> described = info.result().orElseThrow(() -> new IllegalStateException("No result codec defined"));
        var encoded = described.schema().codec().encodeStart(JsonOps.INSTANCE, result);
        return badParams ? encoded.getOrThrow(InvalidParameterJsonRpcException::new) : encoded.getOrThrow(EncodeJsonRpcException::new);
    }

    static <Result> IncomingRpcMethodBuilder<Void, Result> method(ParameterlessRpcMethodFunction<Result> function) {
        return new IncomingRpcMethodBuilder<>(function);
    }

    static <Params, Result> IncomingRpcMethodBuilder<Params, Result> method(RpcMethodFunction<Params, Result> function) {
        return new IncomingRpcMethodBuilder<>(function);
    }

    static <Result> IncomingRpcMethodBuilder<Void, Result> method(Function<MinecraftApi, Result> supplier) {
        return new IncomingRpcMethodBuilder<>(supplier);
    }

    public static record Attributes(boolean runOnMainThread, boolean discoverable, boolean allowPreServerInit) {}

    public static record ParameterlessMethod<Params, Result>(MethodInfo<Params, Result> info, Attributes attributes,
        ParameterlessRpcMethodFunction<Result> supplier) implements IncomingRpcMethod<Params, Result> {
        public JsonElement apply(MinecraftApi minecraftApi, JsonElement paramsJson, ClientInfo clientInfo) {
            boolean empty = paramsJson == null || paramsJson.isJsonArray() && paramsJson.getAsJsonArray().isEmpty();
            if (!empty) {
                throw new InvalidParameterJsonRpcException("Expected no params, or an empty array");
            }
            if (info.params().isPresent()) {
                throw new IllegalArgumentException("Parameterless method must not describe parameters");
            }
            return encodeResult(info, supplier.apply(minecraftApi, clientInfo), true);
        }
    }

    public static record Method<Params, Result>(MethodInfo<Params, Result> info, Attributes attributes, RpcMethodFunction<Params, Result> function)
        implements IncomingRpcMethod<Params, Result> {
        public JsonElement apply(MinecraftApi minecraftApi, JsonElement paramsJson, ClientInfo clientInfo) {
            if (paramsJson == null || !(paramsJson.isJsonArray() || paramsJson.isJsonObject())) {
                throw new InvalidParameterJsonRpcException("Expected params as array or named");
            }
            ParamInfo<Params> expected = info.params()
                .orElseThrow(() -> new IllegalArgumentException("Method defined as having parameters without describing them"));
            JsonElement argument;
            if (paramsJson.isJsonObject()) {
                JsonObject named = paramsJson.getAsJsonObject();
                argument = named.get(expected.name());
                if (argument == null) {
                    throw new InvalidParameterJsonRpcException("Params passed by-name, but expected param [" + expected.name() + "] does not exist");
                }
            } else {
                JsonArray positional = paramsJson.getAsJsonArray();
                if (positional.size() != 1) {
                    throw new InvalidParameterJsonRpcException("Expected exactly one element in the params array");
                }
                argument = positional.get(0);
            }
            Params decoded = expected.schema().codec().parse(JsonOps.INSTANCE, argument).getOrThrow(InvalidParameterJsonRpcException::new);
            return encodeResult(info, function.apply(minecraftApi, decoded, clientInfo), false);
        }
    }

    public static class IncomingRpcMethodBuilder<Params, Result> {
        private String description = "";
        private ParamInfo<Params> paramInfo;
        private ResultInfo<Result> resultInfo;
        private boolean discoverable = true;
        private boolean mainThread = true;
        private boolean preInit;
        private ParameterlessRpcMethodFunction<Result> noArgs;
        private RpcMethodFunction<Params, Result> withArgs;

        public IncomingRpcMethodBuilder(ParameterlessRpcMethodFunction<Result> function) {
            noArgs = function;
        }

        public IncomingRpcMethodBuilder(RpcMethodFunction<Params, Result> function) {
            withArgs = function;
        }

        public IncomingRpcMethodBuilder(Function<MinecraftApi, Result> supplier) {
            noArgs = (api, client) -> supplier.apply(api);
        }

        public IncomingRpcMethodBuilder<Params, Result> description(String description) {
            this.description = description;
            return this;
        }

        public IncomingRpcMethodBuilder<Params, Result> response(String resultName, Schema<Result> resultSchema) {
            resultInfo = new ResultInfo<>(resultName, resultSchema.info());
            return this;
        }

        public IncomingRpcMethodBuilder<Params, Result> param(String paramName, Schema<Params> paramSchema) {
            paramInfo = new ParamInfo<>(paramName, paramSchema.info());
            return this;
        }

        public IncomingRpcMethodBuilder<Params, Result> undiscoverable() {
            discoverable = false;
            return this;
        }

        public IncomingRpcMethodBuilder<Params, Result> notOnMainThread() {
            mainThread = false;
            return this;
        }

        public IncomingRpcMethodBuilder<Params, Result> allowPreServerInit() {
            preInit = true;
            return this;
        }

        public IncomingRpcMethod<Params, Result> build() {
            if (resultInfo == null) {
                throw new IllegalStateException("No response defined");
            }
            Attributes attributes = new Attributes(mainThread, discoverable, preInit);
            MethodInfo<Params, Result> info = new MethodInfo<>(description, paramInfo, resultInfo);
            if (noArgs != null) {
                return new ParameterlessMethod<>(info, attributes, noArgs);
            }
            if (withArgs == null) {
                throw new IllegalStateException("No method defined");
            }
            if (paramInfo == null) {
                throw new IllegalStateException("A method with parameters needs a param schema");
            }
            return new Method<>(info, attributes, withArgs);
        }

        public IncomingRpcMethod<?, ?> register(Registry<IncomingRpcMethod<?, ?>> methodRegistry, String key) {
            return Registry.register(methodRegistry, Identifier.withDefaultNamespace(key), build());
        }
    }

    public interface ParameterlessRpcMethodFunction<Result> {
        Result apply(MinecraftApi api, ClientInfo client);
    }

    public interface RpcMethodFunction<Params, Result> {
        Result apply(MinecraftApi api, Params params, ClientInfo client);
    }
}

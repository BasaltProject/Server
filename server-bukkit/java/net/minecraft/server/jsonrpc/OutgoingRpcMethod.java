// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.jsonrpc;

import com.google.gson.JsonElement;
import com.mojang.serialization.JsonOps;
import net.minecraft.core.Holder;
import net.minecraft.core.Registry;
import net.minecraft.core.registries.BuiltInRegistries;
import net.minecraft.resources.Identifier;
import net.minecraft.server.jsonrpc.api.MethodInfo;
import net.minecraft.server.jsonrpc.api.ParamInfo;
import net.minecraft.server.jsonrpc.api.ResultInfo;
import net.minecraft.server.jsonrpc.api.Schema;

public interface OutgoingRpcMethod<Params, Result> {
    String NOTIFICATION_PREFIX = "notification/";

    MethodInfo<Params, Result> info();

    Attributes attributes();

    default JsonElement encodeParams(Params params) {
        return null;
    }

    default Result decodeResult(JsonElement result) {
        return null;
    }

    private static <P> JsonElement encode(MethodInfo<P, ?> info, P params) {
        ParamInfo<P> described = info.params().orElseThrow(() -> new IllegalStateException("Method defined as having no parameters"));
        return described.schema().codec().encodeStart(JsonOps.INSTANCE, params).getOrThrow();
    }

    private static <R> R decode(MethodInfo<?, R> info, JsonElement raw) {
        ResultInfo<R> described = info.result().orElseThrow(() -> new IllegalStateException("Method defined as having no result"));
        return described.schema().codec().parse(JsonOps.INSTANCE, raw).getOrThrow();
    }

    static OutgoingRpcMethodBuilder<Void, Void> notification() {
        return new OutgoingRpcMethodBuilder<>(ParmeterlessNotification::new);
    }

    static <Params> OutgoingRpcMethodBuilder<Params, Void> notificationWithParams() {
        return new OutgoingRpcMethodBuilder<>(Notification::new);
    }

    static <Result> OutgoingRpcMethodBuilder<Void, Result> request() {
        return new OutgoingRpcMethodBuilder<>(ParameterlessMethod::new);
    }

    static <Params, Result> OutgoingRpcMethodBuilder<Params, Result> requestWithParams() {
        return new OutgoingRpcMethodBuilder<>(Method::new);
    }

    public static record Attributes(boolean discoverable, boolean allowPreServerInit) {}

    public static record ParmeterlessNotification(MethodInfo<Void, Void> info, Attributes attributes) implements OutgoingRpcMethod<Void, Void> {}

    public static record Notification<Params>(MethodInfo<Params, Void> info, Attributes attributes) implements OutgoingRpcMethod<Params, Void> {
        public JsonElement encodeParams(Params params) {
            return encode(info, params);
        }
    }

    public static record ParameterlessMethod<Result>(MethodInfo<Void, Result> info, Attributes attributes) implements OutgoingRpcMethod<Void, Result> {
        public Result decodeResult(JsonElement result) {
            return decode(info, result);
        }
    }

    public static record Method<Params, Result>(MethodInfo<Params, Result> info, Attributes attributes) implements OutgoingRpcMethod<Params, Result> {
        public JsonElement encodeParams(Params params) {
            return encode(info, params);
        }

        public Result decodeResult(JsonElement result) {
            return decode(info, result);
        }
    }

    public static class OutgoingRpcMethodBuilder<Params, Result> {
        public static final Attributes DEFAULT_ATTRIBUTES = new Attributes(true, false);
        private final Factory<Params, Result> factory;
        private String description = "";
        private ParamInfo<Params> paramInfo;
        private ResultInfo<Result> resultInfo;
        private boolean preInit;

        public OutgoingRpcMethodBuilder(Factory<Params, Result> method) {
            this.factory = method;
        }

        public OutgoingRpcMethodBuilder<Params, Result> description(String description) {
            this.description = description;
            return this;
        }

        public OutgoingRpcMethodBuilder<Params, Result> response(String resultName, Schema<Result> resultSchema) {
            resultInfo = new ResultInfo<>(resultName, resultSchema);
            return this;
        }

        public OutgoingRpcMethodBuilder<Params, Result> param(String paramName, Schema<Params> paramSchema) {
            paramInfo = new ParamInfo<>(paramName, paramSchema);
            return this;
        }

        public OutgoingRpcMethodBuilder<Params, Result> allowPreServerInit() {
            preInit = true;
            return this;
        }

        public Holder.Reference<OutgoingRpcMethod<Params, Result>> register(String key) {
            Attributes attributes = preInit ? new Attributes(DEFAULT_ATTRIBUTES.discoverable(), true) : DEFAULT_ATTRIBUTES;
            OutgoingRpcMethod<Params, Result> built = factory.create(new MethodInfo<>(description, paramInfo, resultInfo), attributes);
            return Registry.registerForHolder(BuiltInRegistries.OUTGOING_RPC_METHOD, Identifier.withDefaultNamespace(NOTIFICATION_PREFIX + key), built);
        }
    }

    public interface Factory<Params, Result> {
        OutgoingRpcMethod<Params, Result> create(MethodInfo<Params, Result> info, Attributes attributes);
    }
}

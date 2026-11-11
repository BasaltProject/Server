// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.jsonrpc;

import com.google.gson.JsonElement;
import com.google.gson.JsonNull;
import com.google.gson.JsonObject;

public enum JsonRPCErrors {
    PARSE_ERROR(-32700, "Parse error"),
    INVALID_REQUEST(-32600, "Invalid Request"),
    METHOD_NOT_FOUND(-32601, "Method not found"),
    INVALID_PARAMS(-32602, "Invalid params"),
    INTERNAL_ERROR(-32603, "Internal error");

    private final int code;
    private final String text;

    JsonRPCErrors(int code, String text) {
        this.code = code;
        this.text = text;
    }

    public JsonObject createWithUnknownId(String data) {
        return create(JsonNull.INSTANCE, data);
    }

    public JsonObject createWithoutData(JsonElement id) {
        return create(id, null);
    }

    public JsonObject create(JsonElement id, String data) {
        return JsonRPCUtils.createError(id, text, code, data);
    }
}

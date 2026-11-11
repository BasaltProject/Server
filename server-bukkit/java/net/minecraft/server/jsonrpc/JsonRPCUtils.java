// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.jsonrpc;

import com.google.gson.JsonArray;
import com.google.gson.JsonElement;
import com.google.gson.JsonObject;
import java.util.List;
import net.minecraft.resources.Identifier;
import net.minecraft.util.GsonHelper;

public class JsonRPCUtils {
    public static final String JSON_RPC_VERSION = "2.0";
    public static final String OPEN_RPC_VERSION = "1.3.2";

    private static JsonObject envelope() {
        JsonObject message = new JsonObject();
        message.addProperty("jsonrpc", JSON_RPC_VERSION);
        return message;
    }

    public static JsonObject createSuccessResult(JsonElement id, JsonElement result) {
        JsonObject message = envelope();
        message.add("id", id);
        message.add("result", result);
        return message;
    }

    public static JsonObject createRequest(Integer id, Identifier method, List<JsonElement> params) {
        return createRequest(id, method.toString(), params);
    }

    public static JsonObject createRequest(Integer id, String method, List<JsonElement> params) {
        JsonObject message = envelope();
        if (id != null) {
            message.addProperty("id", id);
        }
        message.addProperty("method", method);
        if (!params.isEmpty()) {
            JsonArray list = new JsonArray();
            params.forEach(list::add);
            message.add("params", list);
        }
        return message;
    }

    public static JsonObject createError(JsonElement id, String message, int errorCode, String data) {
        JsonObject failure = new JsonObject();
        failure.addProperty("code", errorCode);
        failure.addProperty("message", message);
        if (data != null && !data.isBlank()) {
            failure.addProperty("data", data);
        }
        JsonObject reply = envelope();
        reply.add("id", id);
        reply.add("error", failure);
        return reply;
    }

    public static JsonElement getRequestId(JsonObject jsonObject) {
        return jsonObject.get("id");
    }

    public static String getMethodName(JsonObject jsonObject) {
        return GsonHelper.getAsString(jsonObject, "method", null);
    }

    public static JsonElement getParams(JsonObject jsonObject) {
        return jsonObject.get("params");
    }

    public static JsonElement getResult(JsonObject jsonObject) {
        return jsonObject.get("result");
    }

    public static JsonObject getError(JsonObject jsonObject) {
        return GsonHelper.getAsJsonObject(jsonObject, "error", null);
    }
}

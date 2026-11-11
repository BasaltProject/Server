// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.jsonrpc.websocket;

import com.google.gson.JsonElement;
import io.netty.buffer.Unpooled;
import io.netty.channel.ChannelHandlerContext;
import io.netty.handler.codec.MessageToMessageEncoder;
import java.nio.charset.StandardCharsets;
import java.util.List;

/** The websocket framing is native; this hands plugins the serialized text of a message. */
public class JsonToWebSocketEncoder extends MessageToMessageEncoder<JsonElement> {
    protected void encode(ChannelHandlerContext ctx, JsonElement msg, List<Object> out) {
        out.add(Unpooled.copiedBuffer(msg.toString(), StandardCharsets.UTF_8));
    }
}

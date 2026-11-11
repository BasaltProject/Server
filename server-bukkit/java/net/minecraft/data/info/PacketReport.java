// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.data.info;
import com.google.gson.JsonObject;
import java.util.concurrent.CompletableFuture;
import net.minecraft.data.CachedOutput;
import net.minecraft.data.DataProvider;
import net.minecraft.data.PackOutput;
public class PacketReport implements DataProvider {
    private final PackOutput output;

    public PacketReport(PackOutput output) {
        this.output = output;
    }
    public CompletableFuture<?> run(CachedOutput cache) {
        JsonObject root = new JsonObject();
        for (Class<?> holder : new Class<?>[] {net.minecraft.network.protocol.game.GamePacketTypes.class}) {
            JsonObject phase = new JsonObject();
            for (java.lang.reflect.Field field : holder.getFields()) {
                try {
                    if (field.get(null) instanceof net.minecraft.network.protocol.PacketType<?> type) {
                        JsonObject packet = new JsonObject();
                        packet.addProperty("protocol_id", phase.size());
                        phase.add(type.id().toString(), packet);
                    }
                } catch (IllegalAccessException ignored) {
                    continue;
                }
            }
            root.add("play", phase);
        }
        return ReportJson.write(cache, output, "packets.json", root);
    }
    public String getName() {
        return "Packet Report";
    }
}

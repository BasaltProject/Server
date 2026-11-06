// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server;
import com.mojang.serialization.JsonOps;
import com.google.gson.JsonParser;
import java.util.Collection;
import java.util.LinkedHashMap;
import java.util.Map;
import java.util.concurrent.ConcurrentHashMap;
import net.minecraft.advancements.Advancement;
import net.minecraft.advancements.AdvancementHolder;
import net.minecraft.advancements.AdvancementTree;
import net.minecraft.core.HolderLookup;
import net.minecraft.resources.Identifier;
import org.basaltmc.bukkit.MiscNatives;
public class ServerAdvancementManager {
    private final Map<Identifier, AdvancementHolder> cache = new ConcurrentHashMap<>();
    public ServerAdvancementManager(HolderLookup.Provider registries) {}
    public AdvancementHolder get(Identifier id) {
        AdvancementHolder known = cache.get(id);
        if (known != null) {
            return known;
        }
        String[] info = MiscNatives.advInfo(id.toString());
        if (info == null || info.length < 16 || info[15].isEmpty()) {
            return null;
        }
        Advancement parsed = Advancement.CODEC.parse(JsonOps.INSTANCE, JsonParser.parseString(info[15])).result().orElse(null);
        if (parsed == null) {
            return null;
        }
        AdvancementHolder holder = new AdvancementHolder(id, parsed);
        cache.put(id, holder);
        return holder;
    }
    public AdvancementTree tree() {
        AdvancementTree tree = new AdvancementTree();
        tree.addAll(getAllAdvancements());
        return tree;
    }
    public Collection<AdvancementHolder> getAllAdvancements() {
        Map<Identifier, AdvancementHolder> all = new LinkedHashMap<>();
        for (String id : MiscNatives.advIds()) {
            AdvancementHolder holder = get(Identifier.parse(id));
            if (holder != null) {
                all.put(holder.id(), holder);
            }
        }
        return all.values();
    }
}

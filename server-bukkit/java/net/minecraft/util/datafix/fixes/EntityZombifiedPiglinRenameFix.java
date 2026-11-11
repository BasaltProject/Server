// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.datafix.fixes;
import com.mojang.datafixers.schemas.Schema;
import java.util.Map;
public class EntityZombifiedPiglinRenameFix extends SimplestEntityRenameFix {
    public static final Map<String, String> RENAMED_IDS = org.basaltmc.bukkit.facade.DatafixTables.pairs("minecraft:zombie_pigman_spawn_egg=minecraft:zombified_piglin_spawn_egg");
    public EntityZombifiedPiglinRenameFix(Schema outputSchema) {
        super("EntityZombifiedPiglinRenameFix", outputSchema, true);
    }
    protected String rename(String name) {
        return "minecraft:zombie_pigman".equals(name) ? "minecraft:zombified_piglin" : name;
    }
}

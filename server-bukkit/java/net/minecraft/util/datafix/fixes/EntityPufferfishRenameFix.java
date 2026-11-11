// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.datafix.fixes;
import com.mojang.datafixers.schemas.Schema;
import java.util.Map;
public class EntityPufferfishRenameFix extends SimplestEntityRenameFix {
    public static final Map<String, String> RENAMED_IDS = org.basaltmc.bukkit.facade.DatafixTables.pairs("minecraft:puffer_fish_spawn_egg=minecraft:pufferfish_spawn_egg");
    public EntityPufferfishRenameFix(Schema outputSchema, boolean changesType) {
        super("EntityPufferfishRenameFix", outputSchema, changesType);
    }
    protected String rename(String name) {
        return "minecraft:puffer_fish".equals(name) ? "minecraft:pufferfish" : name;
    }
}

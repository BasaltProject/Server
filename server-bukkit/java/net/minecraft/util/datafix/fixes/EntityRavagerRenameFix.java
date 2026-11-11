// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.datafix.fixes;
import com.mojang.datafixers.schemas.Schema;
import java.util.Map;
public class EntityRavagerRenameFix extends SimplestEntityRenameFix {
    public static final Map<String, String> RENAMED_IDS = org.basaltmc.bukkit.facade.DatafixTables.pairs("minecraft:illager_beast_spawn_egg=minecraft:ravager_spawn_egg");
    public EntityRavagerRenameFix(Schema outputSchema, boolean changesType) {
        super("EntityRavagerRenameFix", outputSchema, changesType);
    }
    protected String rename(String name) {
        return "minecraft:illager_beast".equals(name) ? "minecraft:ravager" : name;
    }
}

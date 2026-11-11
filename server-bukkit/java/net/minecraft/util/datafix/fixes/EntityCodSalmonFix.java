// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.datafix.fixes;
import com.mojang.datafixers.schemas.Schema;
import java.util.Map;
public class EntityCodSalmonFix extends SimplestEntityRenameFix {
    public static final Map<String, String> RENAMED_IDS = org.basaltmc.bukkit.facade.DatafixTables.pairs("minecraft:salmon_mob=minecraft:salmon minecraft:cod_mob=minecraft:cod");
    public static final Map<String, String> RENAMED_EGG_IDS = org.basaltmc.bukkit.facade.DatafixTables.pairs("minecraft:salmon_mob_spawn_egg=minecraft:salmon_spawn_egg minecraft:cod_mob_spawn_egg=minecraft:cod_spawn_egg");
    public EntityCodSalmonFix(Schema schema, boolean changesType) {
        super("EntityCodSalmonFix", schema, changesType);
    }
    protected String rename(String name) {
        return RENAMED_IDS.getOrDefault(name, name);
    }
}

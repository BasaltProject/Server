// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.datafix.fixes;
import com.mojang.datafixers.schemas.Schema;
import com.mojang.serialization.Dynamic;
public class LegacyJigsawRotationFix extends LegacyAbstractBlockPropertyFix {
    public LegacyJigsawRotationFix(Schema outputSchema) {
        super(outputSchema, "jigsaw_rotation_fix");
    }
    protected boolean shouldFix(String blockId) {
        return blockId.equals("minecraft:jigsaw");
    }
    protected <T> Dynamic<T> fixProperties(String blockId, Dynamic<T> properties) {
        String facing = properties.get("facing").asString("north");
        String orientation = org.basaltmc.bukkit.facade.DatafixTables.pairs(
            "down=down_south up=up_north north=north_up south=south_up west=west_up east=east_up").getOrDefault(facing, facing);
        return properties.remove("facing").set("orientation", properties.createString(orientation));
    }
}

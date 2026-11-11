// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.datafix.fixes;
import com.mojang.datafixers.DataFix;
import com.mojang.datafixers.TypeRewriteRule;
import com.mojang.datafixers.schemas.Schema;
public class LevelFlatGeneratorInfoFix extends DataFix {
    static final String DEFAULT = "minecraft:bedrock, 2*minecraft:dirt, minecraft:grass_block;1;village";
    public LevelFlatGeneratorInfoFix(Schema outputSchema, boolean changesType) {
        super(outputSchema, changesType);
    }
    public TypeRewriteRule makeRule() {
        return org.basaltmc.bukkit.facade.NativeStepRule.of(getClass().getSimpleName(), getVersionKey());
    }
    String fixString(String generatorOptions) {
        net.minecraft.nbt.CompoundTag level = new net.minecraft.nbt.CompoundTag();
        level.putString("generatorName", "flat");
        level.putString("generatorOptions", generatorOptions);
        return org.basaltmc.bukkit.facade.NativeStep.root("level", getClass().getSimpleName(), getVersionKey(), level).getStringOr("generatorOptions", generatorOptions);
    }
}

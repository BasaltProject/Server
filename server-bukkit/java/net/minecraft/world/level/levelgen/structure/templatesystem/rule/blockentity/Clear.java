// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.structure.templatesystem.rule.blockentity;
import com.mojang.serialization.MapCodec;
import net.minecraft.nbt.CompoundTag;
import net.minecraft.util.RandomSource;
public class Clear implements RuleBlockEntityModifier {
    public static final MapCodec<Clear> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.levelgen.structure.templatesystem.rule.blockentity.Clear.class, "CODEC");
    public CompoundTag apply(RandomSource random, CompoundTag existingTag) {
        return new CompoundTag();
    }
    public RuleBlockEntityModifierType<?> getType() {
        return RuleBlockEntityModifierType.CLEAR;
    }
}

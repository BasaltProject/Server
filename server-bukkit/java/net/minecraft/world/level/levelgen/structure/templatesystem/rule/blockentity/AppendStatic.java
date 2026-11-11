// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.structure.templatesystem.rule.blockentity;
import com.mojang.serialization.MapCodec;
import net.minecraft.nbt.CompoundTag;
import net.minecraft.util.RandomSource;
public class AppendStatic implements RuleBlockEntityModifier {
    private final CompoundTag tag;
    public static final MapCodec<AppendStatic> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.levelgen.structure.templatesystem.rule.blockentity.AppendStatic.class, "CODEC");
    public AppendStatic(CompoundTag tag) {
        this.tag = tag;
    }
    public CompoundTag apply(RandomSource random, CompoundTag existingTag) {
        return existingTag == null ? tag.copy() : existingTag.merge(tag);
    }
    public RuleBlockEntityModifierType<?> getType() {
        return RuleBlockEntityModifierType.APPEND_STATIC;
    }
}

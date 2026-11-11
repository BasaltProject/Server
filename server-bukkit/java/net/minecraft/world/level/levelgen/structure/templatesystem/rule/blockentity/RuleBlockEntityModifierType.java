// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.structure.templatesystem.rule.blockentity;
import com.mojang.serialization.MapCodec;
public interface RuleBlockEntityModifierType<P extends RuleBlockEntityModifier> {
    RuleBlockEntityModifierType<Clear> CLEAR = () -> Clear.CODEC;
    RuleBlockEntityModifierType<Passthrough> PASSTHROUGH = () -> Passthrough.CODEC;
    RuleBlockEntityModifierType<AppendStatic> APPEND_STATIC = () -> AppendStatic.CODEC;
    RuleBlockEntityModifierType<AppendLoot> APPEND_LOOT = () -> AppendLoot.CODEC;
    MapCodec<P> codec();
}

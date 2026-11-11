// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.feature;
import com.mojang.serialization.Codec;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.levelgen.structure.templatesystem.RuleTest;
public record BlockReplacement(RuleTest target, BlockState state) {
    public static final Codec<BlockReplacement> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.level.levelgen.feature.BlockReplacement.class, "CODEC");
    public static BlockReplacement replace(RuleTest rule, BlockState state) {
        return new BlockReplacement(rule, state);
    }
}

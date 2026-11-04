// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.item.component;
import com.mojang.serialization.Codec;
import java.util.List;
import java.util.Optional;
import net.minecraft.core.HolderSet;
import net.minecraft.network.RegistryFriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.world.level.block.Block;
import net.minecraft.world.level.block.state.BlockState;
public record Tool(List<Rule> rules, float defaultMiningSpeed, int damagePerBlock, boolean canDestroyBlocksInCreative) {
    public static final Codec<Tool> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.item.component.Tool.class, "CODEC");
    public static final StreamCodec<RegistryFriendlyByteBuf, Tool> STREAM_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.streamCodec(net.minecraft.world.item.component.Tool.class, "STREAM_CODEC");
    public float getMiningSpeed(BlockState state) {
        for (Rule rule : rules) {
            if (rule.speed().isPresent() && org.basaltmc.bukkit.facade.HolderSets.contains(rule.blocks(), state.getBlock().builtInRegistryHolder())) {
                return rule.speed().get();
            }
        }
        return defaultMiningSpeed;
    }
    public boolean isCorrectForDrops(BlockState state) {
        for (Rule rule : rules) {
            if (rule.correctForDrops().isPresent() && org.basaltmc.bukkit.facade.HolderSets.contains(rule.blocks(), state.getBlock().builtInRegistryHolder())) {
                return rule.correctForDrops().get();
            }
        }
        return false;
    }
    public static record Rule(HolderSet<Block> blocks, Optional<Float> speed, Optional<Boolean> correctForDrops) {
        public static final Codec<Rule> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.item.component.Tool.Rule.class, "CODEC");
        public static final StreamCodec<RegistryFriendlyByteBuf, Rule> STREAM_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.streamCodec(net.minecraft.world.item.component.Tool.Rule.class, "STREAM_CODEC");
        public static Rule minesAndDrops(HolderSet<Block> blocks, float speed) {
            return new Rule(blocks, Optional.of(speed), Optional.of(true));
        }
        public static Rule deniesDrops(HolderSet<Block> blocks) {
            return new Rule(blocks, Optional.empty(), Optional.of(false));
        }
        public static Rule overrideSpeed(HolderSet<Block> blocks, float speed) {
            return new Rule(blocks, Optional.of(speed), Optional.empty());
        }
    }
}

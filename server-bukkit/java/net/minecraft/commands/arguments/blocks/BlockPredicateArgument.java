// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.commands.arguments.blocks;
import com.mojang.datafixers.util.Either;
import net.minecraft.core.registries.Registries;
import net.minecraft.nbt.NbtUtils;
import net.minecraft.world.level.block.entity.BlockEntity;
import com.mojang.brigadier.StringReader;
import com.mojang.brigadier.arguments.ArgumentType;
import com.mojang.brigadier.context.CommandContext;
import com.mojang.brigadier.exceptions.CommandSyntaxException;
import com.mojang.brigadier.suggestion.Suggestions;
import com.mojang.brigadier.suggestion.SuggestionsBuilder;
import java.util.Collection;
import java.util.Map;
import java.util.Set;
import java.util.concurrent.CompletableFuture;
import java.util.function.Predicate;
import net.minecraft.commands.CommandBuildContext;
import net.minecraft.commands.CommandSourceStack;
import net.minecraft.core.HolderLookup;
import net.minecraft.core.HolderSet;
import net.minecraft.nbt.CompoundTag;
import net.minecraft.world.level.block.Block;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.block.state.pattern.BlockInWorld;
import net.minecraft.world.level.block.state.properties.Property;
public class BlockPredicateArgument implements ArgumentType<BlockPredicateArgument.Result> {
    private static final Collection<String> kExamples = java.util.List.of("stone", "minecraft:stone", "stone[foo=bar]", "#stone", "#stone[foo=bar]{baz=nbt}");
    private final HolderLookup<Block> blocks;

    private static boolean nbtMatches(CompoundTag wanted, BlockInWorld at) {
        if (wanted == null) {
            return true;
        }
        BlockEntity entity = at.getEntity();
        return entity != null && NbtUtils.compareNbt(wanted, entity.saveWithFullMetadata(at.getLevel().registryAccess()), true);
    }

    public BlockPredicateArgument(CommandBuildContext context) {
        blocks = context.lookupOrThrow(Registries.BLOCK);
        
    }
    public static BlockPredicateArgument blockPredicate(CommandBuildContext context) {
        return new BlockPredicateArgument(context);
    }
    public Result parse(StringReader reader) throws CommandSyntaxException {
        return parse(blocks, reader);
    }
    public static Result parse(HolderLookup<Block> blocks, StringReader reader) throws CommandSyntaxException {
        Either<BlockStateParser.BlockResult, BlockStateParser.TagResult> parsed = BlockStateParser.parseForTesting(blocks, reader, true);
        return parsed.map(block -> new BlockPredicate(block.blockState(), block.properties().keySet(), block.nbt()),
            tag -> new TagPredicate(tag.tag(), tag.vagueProperties(), tag.nbt()));
    }
    public static Predicate<BlockInWorld> getBlockPredicate(CommandContext<CommandSourceStack> context, String name) throws CommandSyntaxException {
        return context.getArgument(name, Result.class);
    }
    public <S> CompletableFuture<Suggestions> listSuggestions(CommandContext<S> context, SuggestionsBuilder builder) {
        return BlockStateParser.fillSuggestions(blocks, builder, true, true);
    }
    public Collection<String> getExamples() {
        return kExamples;
    }
    static class BlockPredicate implements Result {
        private final BlockState state;
        private final Set<Property<?>> properties;
        private final CompoundTag nbt;

        public BlockPredicate(BlockState state, Set<Property<?>> properties, CompoundTag nbt) {
            this.state = state;
            this.properties = properties;
            this.nbt = nbt;
            
        }
        public boolean test(BlockInWorld blockInWorld) {
            BlockState actual = blockInWorld.getState();
            if (!actual.is(state.getBlock())) {
                return false;
            }
            for (Property<?> property : properties) {
                if (actual.getValue(property) != state.getValue(property)) {
                    return false;
                }
            }
            return nbtMatches(nbt, blockInWorld);
        }
        public boolean requiresNbt() {
            return nbt != null;
        }
    }
    static class TagPredicate implements Result {
        private final HolderSet<Block> tag;
        private final Map<String, String> vagueProperties;
        private final CompoundTag nbt;

        private TagPredicate(HolderSet<Block> tag, Map<String, String> vagueProperties, CompoundTag nbt) {
            this.tag = tag;
            this.vagueProperties = vagueProperties;
            this.nbt = nbt;
            
        }
        public boolean test(BlockInWorld blockInWorld) {
            BlockState actual = blockInWorld.getState();
            if (!actual.is(tag)) {
                return false;
            }
            for (Map.Entry<String, String> wanted : vagueProperties.entrySet()) {
                Property<?> property = actual.getBlock().getStateDefinition().getProperty(wanted.getKey());
                Comparable<?> value = property == null ? null : property.getValue(wanted.getValue()).orElse(null);
                if (value == null || actual.getValue(property) != value) {
                    return false;
                }
            }
            return nbtMatches(nbt, blockInWorld);
        }
        public boolean requiresNbt() {
            return nbt != null;
        }
    }
    public interface Result extends Predicate<BlockInWorld> {
        boolean requiresNbt();
    }
}

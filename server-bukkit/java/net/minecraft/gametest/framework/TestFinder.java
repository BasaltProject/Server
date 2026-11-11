// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.gametest.framework;

import com.mojang.brigadier.context.CommandContext;
import java.util.Collection;
import java.util.List;
import java.util.Optional;
import java.util.function.Supplier;
import java.util.function.UnaryOperator;
import java.util.stream.Stream;
import net.minecraft.commands.CommandSourceStack;
import net.minecraft.core.BlockPos;
import net.minecraft.core.GlobalPos;
import net.minecraft.core.Holder;
import net.minecraft.core.registries.Registries;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.level.block.entity.TestInstanceBlockEntity;

public class TestFinder implements TestInstanceFinder, TestPosFinder {
    private static final int kNearbyRadius = 15;
    private static final int kFullRadius = 250;
    private final CommandSourceStack source;
    private final TestInstanceFinder testInstanceFinder;
    private final TestPosFinder testPosFinder;

    private TestFinder(CommandSourceStack source, TestInstanceFinder testInstanceFinder, TestPosFinder testPosFinder) {
        this.source = source;
        this.testInstanceFinder = testInstanceFinder;
        this.testPosFinder = testPosFinder;
    }

    @Override
    public Stream<GlobalPos> findTestPos() {
        return testPosFinder.findTestPos();
    }

    public static Builder builder() {
        return new Builder();
    }

    public CommandSourceStack source() {
        return source;
    }

    @Override
    public Stream<Holder.Reference<GameTestInstance>> findTests() {
        return testInstanceFinder.findTests();
    }

    public static class Builder {
        private final UnaryOperator<Supplier<Stream<Holder.Reference<GameTestInstance>>>> testFinderWrapper;
        private final UnaryOperator<Supplier<Stream<GlobalPos>>> positionWrapper;

        public Builder() {
            this.testFinderWrapper = finder -> finder;
            this.positionWrapper = finder -> finder;
        }

        private Builder(UnaryOperator<Supplier<Stream<Holder.Reference<GameTestInstance>>>> testFinderWrapper, UnaryOperator<Supplier<Stream<GlobalPos>>> structureBlockPosFinderWrapper) {
            this.testFinderWrapper = testFinderWrapper;
            this.positionWrapper = structureBlockPosFinderWrapper;
        }

        public Builder createMultipleCopies(int amount) {
            return new Builder(finder -> () -> {
                List<Holder.Reference<GameTestInstance>> once = finder.get().toList();
                return Stream.generate(once::stream).limit(amount).flatMap(s -> s);
            }, positionWrapper);
        }

        private TestFinder assemble(CommandSourceStack source, Supplier<Stream<GlobalPos>> positions) {
            Supplier<Stream<GlobalPos>> wrappedPositions = positionWrapper.apply(positions);
            Supplier<Stream<Holder.Reference<GameTestInstance>>> tests = () -> wrappedPositions.get()
                .map(global -> testAt(source, global)).flatMap(Optional::stream);
            Supplier<Stream<Holder.Reference<GameTestInstance>>> wrappedTests = testFinderWrapper.apply(tests);
            return new TestFinder(source, wrappedTests::get, wrappedPositions::get);
        }

        private static Optional<Holder.Reference<GameTestInstance>> testAt(CommandSourceStack source, GlobalPos global) {
            ServerLevel level = source.getServer().getLevel(global.dimension());
            if (level == null || !(level.getBlockEntity(global.pos()) instanceof TestInstanceBlockEntity entity)) {
                return Optional.empty();
            }
            return entity.test().flatMap(key -> level.registryAccess().lookupOrThrow(Registries.TEST_INSTANCE).get(key));
        }

        private static Stream<GlobalPos> around(CommandSourceStack source, BlockPos center, int radius) {
            ServerLevel level = source.getLevel();
            return StructureUtils.findTestBlocks(center, radius, level).map(pos -> new GlobalPos(level.dimension(), pos));
        }

        public TestFinder radius(CommandContext<CommandSourceStack> sourceStack, int radius) {
            CommandSourceStack source = sourceStack.getSource();
            return assemble(source, () -> around(source, BlockPos.containing(source.getPosition()), radius));
        }

        public TestFinder nearest(CommandContext<CommandSourceStack> sourceStack) {
            CommandSourceStack source = sourceStack.getSource();
            BlockPos here = BlockPos.containing(source.getPosition());
            return assemble(source, () -> StructureUtils.findNearestTest(here, kNearbyRadius, source.getLevel())
                .map(pos -> new GlobalPos(source.getLevel().dimension(), pos)).stream());
        }

        public TestFinder allNearby(CommandContext<CommandSourceStack> sourceStack) {
            CommandSourceStack source = sourceStack.getSource();
            return assemble(source, () -> around(source, BlockPos.containing(source.getPosition()), kFullRadius));
        }

        public TestFinder lookedAt(CommandContext<CommandSourceStack> sourceStack) {
            CommandSourceStack source = sourceStack.getSource();
            return assemble(source, () -> source.getEntity() == null ? Stream.empty()
                : StructureUtils.lookedAtTestPos(BlockPos.containing(source.getPosition()), source.getEntity(), source.getLevel())
                    .map(pos -> new GlobalPos(source.getLevel().dimension(), pos)));
        }

        public TestFinder failedTests(CommandContext<CommandSourceStack> sourceStack, boolean onlyRequiredTests) {
            CommandSourceStack source = sourceStack.getSource();
            Supplier<Stream<Holder.Reference<GameTestInstance>>> tests = () ->
                FailedTestTracker.getLastFailedTests().filter(test -> !onlyRequiredTests || test.value().required());
            Supplier<Stream<Holder.Reference<GameTestInstance>>> wrapped = testFinderWrapper.apply(tests);
            return new TestFinder(source, wrapped::get, Stream::empty);
        }

        public TestFinder byResourceSelection(CommandContext<CommandSourceStack> sourceStack, Collection<Holder.Reference<GameTestInstance>> holders) {
            Supplier<Stream<Holder.Reference<GameTestInstance>>> tests = holders::stream;
            Supplier<Stream<Holder.Reference<GameTestInstance>>> wrapped = testFinderWrapper.apply(tests);
            return new TestFinder(sourceStack.getSource(), wrapped::get, Stream::empty);
        }

        public TestFinder failedTests(CommandContext<CommandSourceStack> sourceStack) {
            return failedTests(sourceStack, false);
        }
    }
}

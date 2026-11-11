// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.gametest.framework;

import com.mojang.brigadier.CommandDispatcher;
import com.mojang.brigadier.arguments.IntegerArgumentType;
import com.mojang.brigadier.arguments.StringArgumentType;
import com.mojang.brigadier.context.CommandContext;
import com.mojang.brigadier.suggestion.Suggestions;
import com.mojang.brigadier.suggestion.SuggestionsBuilder;
import java.util.List;
import java.util.concurrent.CompletableFuture;
import net.minecraft.ChatFormatting;
import net.minecraft.commands.CommandBuildContext;
import net.minecraft.commands.CommandSourceStack;
import net.minecraft.commands.Commands;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Holder;
import net.minecraft.core.registries.Registries;
import net.minecraft.network.chat.Component;
import net.minecraft.server.level.ServerPlayer;
import net.minecraft.world.level.block.entity.TestInstanceBlockEntity;

public class TestCommand {
    public static final int TEST_NEARBY_SEARCH_RADIUS = 15;
    public static final int TEST_FULL_SEARCH_RADIUS = 250;
    public static final int VERIFY_TEST_GRID_AXIS_SIZE = 10;
    public static final int VERIFY_TEST_BATCH_SIZE = 100;

    public static void register(CommandDispatcher<CommandSourceStack> dispatcher, CommandBuildContext context) {
        dispatcher.register(Commands.literal("test").requires(Commands.hasPermission(Commands.LEVEL_GAMEMASTERS))
            .then(Commands.literal("run").then(Commands.argument("test", StringArgumentType.greedyString())
                .suggests(TestCommand::suggestTestFunction).executes(ctx -> runNamed(ctx, 1))))
            .then(Commands.literal("runmultiple").then(Commands.argument("amount", IntegerArgumentType.integer(1))
                .then(Commands.argument("test", StringArgumentType.greedyString()).suggests(TestCommand::suggestTestFunction)
                    .executes(ctx -> runNamed(ctx, IntegerArgumentType.getInteger(ctx, "amount"))))))
            .then(Commands.literal("runall").executes(ctx -> runFinder(ctx, TestFinder.builder().allNearby(ctx))))
            .then(Commands.literal("runthese").executes(ctx -> runFinder(ctx, TestFinder.builder().radius(ctx, TEST_NEARBY_SEARCH_RADIUS))))
            .then(Commands.literal("runclosest").executes(ctx -> runFinder(ctx, TestFinder.builder().nearest(ctx))))
            .then(Commands.literal("runfailed").executes(ctx -> runFinder(ctx, TestFinder.builder().failedTests(ctx))))
            .then(Commands.literal("stop").executes(ctx -> {
                GameTestTicker.SINGLETON.clear();
                ctx.getSource().sendSuccess(() -> Component.translatable("commands.test.stopped"), true);
                return 1;
            }))
            .then(Commands.literal("clearall").executes(ctx -> clearAll(ctx.getSource())))
            .then(Commands.literal("resetclosest").executes(ctx -> reset(ctx, TestFinder.builder().nearest(ctx))))
            .then(Commands.literal("resetthese").executes(ctx -> reset(ctx, TestFinder.builder().radius(ctx, TEST_NEARBY_SEARCH_RADIUS))))
            .then(Commands.literal("locate").then(Commands.argument("test", StringArgumentType.greedyString())
                .suggests(TestCommand::suggestTestFunction).executes(TestCommand::locate)))
            .then(Commands.literal("export").executes(ctx -> exportNearest(ctx))));
    }

    public static CompletableFuture<Suggestions> suggestTestFunction(CommandContext<CommandSourceStack> context, SuggestionsBuilder builder) {
        context.getSource().registryAccess().lookupOrThrow(Registries.TEST_INSTANCE).listElementIds()
            .map(key -> key.identifier().toString()).forEach(builder::suggest);
        return builder.buildFuture();
    }

    private static int runNamed(CommandContext<CommandSourceStack> ctx, int copies) {
        List<Holder.Reference<GameTestInstance>> tests = GameTestServer
            .getTestsForSelection(ctx.getSource().registryAccess(), StringArgumentType.getString(ctx, "test")).toList();
        if (tests.isEmpty()) {
            ctx.getSource().sendFailure(Component.translatable("commands.test.no_tests"));
            return 0;
        }
        return runFinder(ctx, TestFinder.builder().createMultipleCopies(copies).byResourceSelection(ctx, tests));
    }

    private static int runFinder(CommandContext<CommandSourceStack> ctx, TestFinder finder) {
        CommandSourceStack source = ctx.getSource();
        List<GameTestInfo> infos = finder.findTests().map(test -> new GameTestInfo(test, net.minecraft.world.level.block.Rotation.NONE, source.getLevel(), RetryOptions.noRetries())).toList();
        if (infos.isEmpty()) {
            source.sendFailure(Component.translatable("commands.test.no_tests"));
            return 0;
        }
        BlockPos origin = BlockPos.containing(source.getPosition());
        int surfaceY = source.getLevel().getHeight(net.minecraft.world.level.levelgen.Heightmap.Types.WORLD_SURFACE, origin.getX(), origin.getZ());
        GameTestRunner runner = GameTestRunner.Builder.fromInfo(infos, source.getServer())
            .newStructureSpawner(new StructureGridSpawner(dimension -> new BlockPos(origin.getX() + 2, surfaceY, origin.getZ() + 2), GameTestRunner.DEFAULT_TESTS_PER_ROW, false))
            .build();
        return trackAndStartRunner(source, runner);
    }

    public static int trackAndStartRunner(CommandSourceStack source, GameTestRunner runner) {
        runner.addListener(new TestBatchSummaryDisplayer(source));
        MultipleTestTracker tracker = new MultipleTestTracker(runner.getTestInfos());
        tracker.addListener(new TestSummaryDisplayer(source, tracker));
        tracker.addFailureListener(info -> FailedTestTracker.rememberFailedTest(info.getTestHolder()));
        runner.start();
        source.sendSuccess(() -> Component.translatable("commands.test.run.started", runner.getTestInfos().size()), true);
        return runner.getTestInfos().size();
    }

    private static int clearAll(CommandSourceStack source) {
        int cleared = 0;
        for (BlockPos pos : (Iterable<BlockPos>) StructureUtils.findTestBlocks(BlockPos.containing(source.getPosition()), TEST_FULL_SEARCH_RADIUS, source.getLevel())::iterator) {
            if (source.getLevel().getBlockEntity(pos) instanceof TestInstanceBlockEntity entity) {
                StructureUtils.clearSpaceForStructure(entity.getTestBoundingBox(), source.getLevel());
                source.getLevel().destroyBlock(pos, false);
                cleared++;
            }
        }
        int total = cleared;
        source.sendSuccess(() -> Component.translatable("commands.test.clear.success", total), true);
        return cleared;
    }

    private static int reset(CommandContext<CommandSourceStack> ctx, TestFinder finder) {
        CommandSourceStack source = ctx.getSource();
        int[] count = {0};
        finder.findTestPos().forEach(global -> {
            if (source.getLevel().getBlockEntity(global.pos()) instanceof TestInstanceBlockEntity entity) {
                entity.resetTest(message -> source.sendSuccess(() -> message, true));
                count[0]++;
            }
        });
        return count[0];
    }

    private static int locate(CommandContext<CommandSourceStack> ctx) {
        CommandSourceStack source = ctx.getSource();
        String wanted = StringArgumentType.getString(ctx, "test");
        BlockPos here = BlockPos.containing(source.getPosition());
        var found = StructureUtils.findTestBlocks(here, TEST_FULL_SEARCH_RADIUS, source.getLevel())
            .filter(pos -> source.getLevel().getBlockEntity(pos) instanceof TestInstanceBlockEntity entity
                && entity.test().map(key -> key.identifier().toString().equals(wanted) || key.identifier().getPath().equals(wanted)).orElse(false))
            .findFirst();
        if (found.isEmpty()) {
            source.sendFailure(Component.translatable("commands.test.locate.not_found", wanted));
            return 0;
        }
        source.sendSuccess(() -> Component.translatable("commands.test.locate.found", found.get().toShortString()), true);
        return 1;
    }

    private static int exportNearest(CommandContext<CommandSourceStack> ctx) {
        CommandSourceStack source = ctx.getSource();
        var nearest = StructureUtils.findNearestTest(BlockPos.containing(source.getPosition()), TEST_NEARBY_SEARCH_RADIUS, source.getLevel());
        if (nearest.isEmpty() || !(source.getLevel().getBlockEntity(nearest.get()) instanceof TestInstanceBlockEntity entity)) {
            source.sendFailure(Component.translatable("commands.test.no_tests"));
            return 0;
        }
        return entity.exportTest(message -> source.sendSuccess(() -> message, true)) ? 1 : 0;
    }

    static record Info(ServerPlayer player, BlockPos playerPos, BlockPos testPos, int surfaceY) {
    }

    public static final class TestSummaryDisplayer implements GameTestListener {
        private final CommandSourceStack source;
        private final MultipleTestTracker tracker;

        public TestSummaryDisplayer(CommandSourceStack source, MultipleTestTracker tracker) {
            this.source = source;
            this.tracker = tracker;
        }

        private void progress() {
            if (tracker.isDone()) {
                int failedRequired = tracker.getFailedRequiredCount();
                int failedOptional = tracker.getFailedOptionalCount();
                ChatFormatting color = failedRequired > 0 ? ChatFormatting.RED : ChatFormatting.GREEN;
                source.sendSuccess(() -> Component.literal("Finished " + tracker.getTotalCount() + " test(s): " + failedRequired
                    + " required and " + failedOptional + " optional failed").withStyle(color), true);
            }
        }

        @Override
        public void testStructureLoaded(GameTestInfo testInfo) {
        }

        @Override
        public void testPassed(GameTestInfo testInfo, GameTestRunner runner) {
            progress();
        }

        @Override
        public void testFailed(GameTestInfo testInfo, GameTestRunner runner) {
            progress();
        }

        @Override
        public void testAddedForRerun(GameTestInfo original, GameTestInfo copy, GameTestRunner runner) {
            tracker.addTestToTrack(copy);
        }
    }

    static record TestBatchSummaryDisplayer(CommandSourceStack source) implements GameTestBatchListener {
        @Override
        public void testBatchStarting(GameTestBatch batch) {
            source.sendSuccess(() -> Component.literal("Starting batch " + batch.index() + " (" + batch.gameTestInfos().size() + " tests)"), false);
        }

        @Override
        public void testBatchFinished(GameTestBatch batch) {
            source.sendSuccess(() -> Component.literal("Finished batch " + batch.index()), false);
        }
    }
}

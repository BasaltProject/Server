// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.gametest.framework;

import com.mojang.serialization.MapCodec;
import com.mojang.serialization.codecs.RecordCodecBuilder;
import java.util.ArrayList;
import java.util.List;
import java.util.function.Consumer;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Holder;
import net.minecraft.network.chat.Component;
import net.minecraft.network.chat.MutableComponent;
import net.minecraft.world.level.block.Blocks;
import net.minecraft.world.level.block.entity.TestBlockEntity;
import net.minecraft.world.level.block.state.properties.TestBlockMode;

public class BlockBasedTestInstance extends GameTestInstance {
    public static final MapCodec<BlockBasedTestInstance> CODEC = RecordCodecBuilder.mapCodec(i -> i.group(
        TestData.CODEC.forGetter(GameTestInstance::info)).apply(i, BlockBasedTestInstance::new));

    public BlockBasedTestInstance(TestData<Holder<TestEnvironmentDefinition<?>>> testData) {
        super(testData);
    }

    @Override
    public void run(GameTestHelper helper) {
        BlockPos start = findStartBlock(helper);
        helper.getBlockEntity(start, TestBlockEntity.class).trigger();
        helper.onEachTick(() -> {
            List<BlockPos> accepts = findTestBlocks(helper, TestBlockMode.ACCEPT);
            if (accepts.isEmpty()) {
                helper.fail(Component.translatable("test_block.error.missing", TestBlockMode.ACCEPT.getDisplayName()));
            }
            if (accepts.stream().anyMatch(pos -> helper.getBlockEntity(pos, TestBlockEntity.class).hasTriggered())) {
                helper.succeed();
                return;
            }
            forTriggered(helper, TestBlockMode.FAIL, entity -> helper.fail(Component.literal(entity.getMessage())));
            forTriggered(helper, TestBlockMode.LOG, TestBlockEntity::trigger);
        });
    }

    private void forTriggered(GameTestHelper helper, TestBlockMode mode, Consumer<TestBlockEntity> action) {
        for (BlockPos pos : findTestBlocks(helper, mode)) {
            TestBlockEntity entity = helper.getBlockEntity(pos, TestBlockEntity.class);
            if (entity.hasTriggered()) {
                action.accept(entity);
                entity.reset();
            }
        }
    }

    private BlockPos findStartBlock(GameTestHelper helper) {
        List<BlockPos> starts = findTestBlocks(helper, TestBlockMode.START);
        if (starts.isEmpty()) {
            helper.fail(Component.translatable("test_block.error.missing", TestBlockMode.START.getDisplayName()));
        }
        if (starts.size() != 1) {
            helper.fail(Component.translatable("test_block.error.too_many", TestBlockMode.START.getDisplayName()));
        }
        return starts.getFirst();
    }

    private List<BlockPos> findTestBlocks(GameTestHelper helper, TestBlockMode mode) {
        List<BlockPos> found = new ArrayList<>();
        helper.forEveryBlockInStructure(pos -> {
            if (helper.getBlockState(pos).is(Blocks.TEST_BLOCK)
                && helper.getLevel().getBlockEntity(helper.absolutePos(pos)) instanceof TestBlockEntity entity && entity.getMode() == mode) {
                found.add(pos.immutable());
            }
        });
        return found;
    }

    @Override
    public MapCodec<BlockBasedTestInstance> codec() {
        return CODEC;
    }

    @Override
    protected MutableComponent typeDescription() {
        return Component.translatable("test_instance.type.block_based");
    }
}

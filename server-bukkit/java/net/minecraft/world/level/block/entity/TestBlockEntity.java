// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.block.entity;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.level.block.Blocks;
import net.minecraft.world.level.block.TestBlock;
import org.basaltmc.bukkit.facade.ValueIo;
import net.minecraft.core.BlockPos;
import net.minecraft.core.HolderLookup;
import net.minecraft.nbt.CompoundTag;
import net.minecraft.network.protocol.game.ClientboundBlockEntityDataPacket;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.block.state.properties.TestBlockMode;
import net.minecraft.world.level.storage.ValueInput;
import net.minecraft.world.level.storage.ValueOutput;
public class TestBlockEntity extends BlockEntity {
    private boolean triggered;
    public TestBlockEntity(ServerLevel level, BlockPos pos, CompoundTag nbt) {
        super(level, pos, nbt);
    }
    private void put(String key, Object value) {
        CompoundTag tag = nbt();
        if (value instanceof String text) {
            tag.putString(key, text);
        } else if (value instanceof Boolean flag) {
            tag.putBoolean(key, flag);
        }
        load(tag);
    }
    public TestBlockEntity(BlockPos worldPosition, BlockState blockState) {
        super(BlockEntityType.named("test_block"), worldPosition, blockState);
        put("mode", blockState.getValue(TestBlock.MODE).getSerializedName());
    }
    protected void saveAdditional(ValueOutput output) {
        ValueIo.write(output, saveWithoutMetadata((HolderLookup.Provider) null));
    }
    protected void loadAdditional(ValueInput input) {
        load(ValueIo.read(input));
    }
    public ClientboundBlockEntityDataPacket getUpdatePacket() {
        return ClientboundBlockEntityDataPacket.create(this);
    }
    public CompoundTag getUpdateTag(HolderLookup.Provider registries) {
        return saveCustomOnly(registries);
    }
    public boolean isPowered() {
        return nbt().getBooleanOr("powered", false);
    }
    public void setPowered(boolean powered) {
        put("powered", powered);
    }
    public TestBlockMode getMode() {
        String stored = nbt().getStringOr("mode", "fail");
        for (TestBlockMode kind : TestBlockMode.values()) {
            if (kind.getSerializedName().equals(stored)) {
                return kind;
            }
        }
        return TestBlockMode.FAIL;
    }
    public void setMode(TestBlockMode mode) {
        put("mode", mode.getSerializedName());
        if (getLevel() != null && getLevel().getBlockState(getBlockPos()).is(Blocks.TEST_BLOCK)) {
            getLevel().setBlock(getBlockPos(), getLevel().getBlockState(getBlockPos()).setValue(TestBlock.MODE, mode), 2);
        }
    }
    public void reset() {
        triggered = false;
        if (getMode() == TestBlockMode.START && getLevel() != null) {
            setPowered(false);
            getLevel().updateNeighborsAt(getBlockPos(), getBlockState().getBlock());
        }
    }
    public void trigger() {
        if (getMode() == TestBlockMode.START && getLevel() != null) {
            setPowered(true);
            getLevel().updateNeighborsAt(getBlockPos(), getBlockState().getBlock());
            log();
            return;
        }
        if (getMode() == TestBlockMode.LOG) {
            log();
        }
        triggered = true;
    }
    public void log() {
        if (!getMessage().isBlank()) {
            org.slf4j.LoggerFactory.getLogger("TestBlock").info("Test {} (at {}): {}", getMode().getSerializedName(), getBlockPos(), getMessage());
        }
    }
    public boolean hasTriggered() {
        return triggered;
    }
    public String getMessage() {
        return nbt().getStringOr("message", "");
    }
    public void setMessage(String message) {
        put("message", message);


    }
}

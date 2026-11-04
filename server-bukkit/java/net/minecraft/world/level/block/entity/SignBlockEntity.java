// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.block.entity;

import java.util.UUID;
import java.util.function.UnaryOperator;
import net.minecraft.core.BlockPos;
import net.minecraft.core.HolderLookup;
import net.minecraft.nbt.CompoundTag;
import net.minecraft.nbt.NbtOps;
import net.minecraft.nbt.Tag;
import net.minecraft.resources.Identifier;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.storage.ValueInput;
import net.minecraft.world.level.storage.ValueOutput;
import org.basaltmc.bukkit.facade.ValueIo;

public class SignBlockEntity extends BlockEntity {
    private static final String kWaxedKey = "is_waxed";
    private UUID playerWhoMayEdit;

    protected SignBlockEntity() {
    }

    public SignBlockEntity(BlockPos worldPosition, BlockState blockState) {
        this(BlockEntityType.named("sign"), worldPosition, blockState);
    }

    public SignBlockEntity(BlockEntityType<? extends SignBlockEntity> type, BlockPos worldPosition, BlockState blockState) {
        super(type, worldPosition, blockState);
    }

    public SignBlockEntity(ServerLevel level, BlockPos pos, CompoundTag nbt) {
        super(level, pos, nbt);
    }

    private static String textKey(SignTextSlot slot) {
        return slot == SignTextSlot.FRONT ? "front_text" : "back_text";
    }

    public SignText getText(SignTextSlot slot) {
        Tag stored = nbt().get(textKey(slot));
        return stored == null ? SignText.EMPTY : SignText.CODEC.parse(NbtOps.INSTANCE, stored).result().orElse(SignText.EMPTY);
    }

    public int getTextLineHeight() {
        return 10;
    }

    public int getMaxTextLineWidth() {
        return 90;
    }

    @Override
    protected void saveAdditional(ValueOutput output) {
        ValueIo.write(output, saveWithoutMetadata((HolderLookup.Provider) null));
    }

    @Override
    protected void loadAdditional(ValueInput input) {
        load(ValueIo.read(input));
    }

    public boolean updateText(UnaryOperator<SignText> function, SignTextSlot slot) {
        SignText current = getText(slot);
        return setTextChecked(function.apply(current), slot);
    }

    public void setText(SignText text, SignTextSlot slot) {
        setTextChecked(text, slot);
    }

    private boolean setTextChecked(SignText text, SignTextSlot slot) {
        if (isWaxed()) {
            return false;
        }
        SignText.CODEC.encodeStart(NbtOps.INSTANCE, text).result().ifPresent(encoded -> {
            CompoundTag tag = nbt();
            tag.put(textKey(slot), encoded);
            load(tag);
        });
        return true;
    }

    public boolean canExecuteClickCommands(SignTextSlot slot, net.minecraft.world.entity.player.Player player) {
        return isWaxed() && getText(slot).hasAnyClickCommands(false);
    }

    public CompoundTag getUpdateTag(HolderLookup.Provider registries) {
        return saveWithoutMetadata(registries);
    }

    public void setAllowedPlayerEditor(UUID playerUUID) {
        this.playerWhoMayEdit = playerUUID;
    }

    public UUID getPlayerWhoMayEdit() {
        return playerWhoMayEdit;
    }

    public boolean isWaxed() {
        return nbt().getBooleanOr(kWaxedKey, false);
    }

    public boolean setWaxed(boolean isWaxed) {
        if (isWaxed() == isWaxed) {
            return false;
        }
        CompoundTag tag = nbt();
        tag.putBoolean(kWaxedKey, isWaxed);
        load(tag);
        return true;
    }
}

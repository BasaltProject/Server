// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.animal.equine;
import net.minecraft.world.entity.ai.attributes.Attributes;
import net.minecraft.network.syncher.SynchedEntityData;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.util.RandomSource;
import net.minecraft.world.InteractionHand;
import net.minecraft.world.InteractionResult;
import net.minecraft.world.entity.EntityDimensions;
import net.minecraft.world.entity.EntityType;
import net.minecraft.world.entity.Pose;
import net.minecraft.world.entity.SlotAccess;
import net.minecraft.world.entity.ai.attributes.AttributeSupplier;
import net.minecraft.world.entity.player.Player;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.storage.ValueInput;
import net.minecraft.world.level.storage.ValueOutput;
import net.minecraft.world.phys.Vec3;
public abstract class AbstractChestedHorse extends AbstractHorse {
    protected AbstractChestedHorse(EntityType<? extends AbstractChestedHorse> type, Level level) {
        super(type, level);
    }
    protected void randomizeAttributes(RandomSource random) {
        }
    protected void defineSynchedData(SynchedEntityData.Builder entityData) {
    }
    public static AttributeSupplier.Builder createBaseChestedHorseAttributes() {
        return createBaseHorseAttributes().add(Attributes.MOVEMENT_SPEED, 0.175F).add(Attributes.JUMP_STRENGTH, 0.5);
    }
    public boolean hasChest() {
        return live() instanceof org.bukkit.entity.ChestedHorse api && api.isCarryingChest();
    }
    public void setChest(boolean flag) {
        if (live() instanceof org.bukkit.entity.ChestedHorse api) {
            api.setCarryingChest(flag);
        }
    }
    public EntityDimensions getDefaultDimensions(Pose pose) {
        return net.minecraft.world.entity.EntityDimensions.scalable(getBbWidth(), getBbHeight());
    }
    protected void dropEquipment(ServerLevel level) {
    }
    protected void addAdditionalSaveData(ValueOutput output) {
    }
    protected void readAdditionalSaveData(ValueInput input) {
    }
    public SlotAccess getSlot(int slot) {
        return super.getSlot(slot);
    }
    public InteractionResult mobInteract(Player player, InteractionHand hand) {
        return net.minecraft.world.InteractionResult.PASS;
    }
    public Vec3[] getQuadLeashOffsets() {
        return net.minecraft.world.entity.Leashable.createQuadLeashOffsets(this, 0.04, 0.41, 0.18, 0.73);
    }
    protected void playChestEquipsSound() {
        }
    public int getInventoryColumns() {
        return hasChest() ? 5 : 0;
    }
}

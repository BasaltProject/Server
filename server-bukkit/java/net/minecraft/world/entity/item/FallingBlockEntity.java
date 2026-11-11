// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.item;
import net.minecraft.world.level.block.Blocks;
import net.minecraft.world.level.block.Fallable;
import net.minecraft.world.level.block.AnvilBlock;
import net.minecraft.world.level.block.state.properties.BlockStateProperties;
import net.minecraft.world.entity.EntitySelector;
import net.minecraft.world.entity.EntityTypes;
import net.minecraft.util.Mth;
import net.minecraft.tags.BlockTags;
import net.minecraft.CrashReportCategory;
import net.minecraft.core.BlockPos;
import net.minecraft.nbt.CompoundTag;
import net.minecraft.network.chat.Component;
import net.minecraft.network.protocol.Packet;
import net.minecraft.network.protocol.game.ClientGamePacketListener;
import net.minecraft.network.protocol.game.ClientboundAddEntityPacket;
import net.minecraft.network.syncher.EntityDataAccessor;
import net.minecraft.network.syncher.SynchedEntityData;
import net.minecraft.server.level.ServerEntity;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.damagesource.DamageSource;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.entity.EntityType;
import net.minecraft.world.entity.MoveSimulationType;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.block.Block;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.portal.TeleportTransition;
import net.minecraft.world.level.storage.ValueInput;
import net.minecraft.world.level.storage.ValueOutput;
public class FallingBlockEntity extends Entity {
    private BlockState blockState = Blocks.SAND.defaultBlockState();
    private boolean cancelDrop;
    private boolean hurtEntities;
    private int fallDamageMax = 40;
    private float fallDamagePerDistance;
    private BlockPos startPos = BlockPos.ZERO;
    public int time;
    public boolean dropItem;
    public CompoundTag blockData;
    public boolean forceTickAfterTeleportToDuplicate;
    protected static final EntityDataAccessor<BlockPos> DATA_START_POS = net.minecraft.network.syncher.EntityDataSerializers.BLOCK_POS.createAccessor(8);
    public FallingBlockEntity(EntityType<? extends FallingBlockEntity> type, Level level) {
        super(type, level);
    }
    private FallingBlockEntity(Level level, double x, double y, double z, BlockState blockState) {
        super(EntityTypes.FALLING_BLOCK, level);
        this.blockState = blockState;
        this.dropItem = true;
        setPos(x, y, z);
    }
    public static FallingBlockEntity fall(Level level, BlockPos pos, BlockState state) {
        BlockState shown = state.hasProperty(BlockStateProperties.WATERLOGGED) ? state.setValue(BlockStateProperties.WATERLOGGED, false) : state;
        FallingBlockEntity entity = new FallingBlockEntity(level, pos.getX() + 0.5, pos.getY(), pos.getZ() + 0.5, shown);
        level.setBlockAndUpdate(pos, state.getFluidState().createLegacyBlock());
        level.addFreshEntity(entity);
        return entity;
    }
    public boolean isAttackable() {
        return false;
    }
    public final boolean hurtServer(ServerLevel level, DamageSource source, float damage) {
        return super.hurtServer(level, source, damage);
    }
    public void setStartPos(BlockPos pos) {
        this.startPos = pos;
    }
    public BlockPos getStartPos() {
        return startPos;
    }
    protected Entity.MovementEmission getMovementEmission() {
        return super.getMovementEmission();
    }
    protected void defineSynchedData(SynchedEntityData.Builder entityData) {
    }
    public boolean isPickable() {
        return super.isPickable();
    }
    protected double getDefaultGravity() {
        return super.getDefaultGravity();
    }
    public void tick() {
    }
    public void callOnBrokenAfterFall(Block block, BlockPos pos) {
        if (block instanceof Fallable fallable) {
            fallable.onBrokenAfterFall(level(), pos, this);
        }
    }
    public boolean causeFallDamage(double fallDistance, float damageModifier, DamageSource damageSource) {
        if (!hurtEntities) {
            return false;
        }
        int blocks = Mth.ceil(fallDistance - 1.0);
        if (blocks < 0) {
            return false;
        }
        DamageSource cause = blockState.getBlock() instanceof Fallable fallable ? fallable.getFallDamageSource(this) : damageSources().fallingBlock(this);
        float damage = Math.min(Mth.floor(blocks * fallDamagePerDistance), fallDamageMax);
        level().getEntities(this, getBoundingBox(), EntitySelector.NO_CREATIVE_OR_SPECTATOR.and(EntitySelector.LIVING_ENTITY_STILL_ALIVE))
            .forEach(victim -> victim.hurt(cause, damage));
        if (blockState.is(BlockTags.ANVIL) && damage > 0.0F && random.nextFloat() < 0.05F + blocks * 0.05F) {
            BlockState worn = AnvilBlock.damage(blockState);
            if (worn == null) {
                cancelDrop = true;
            } else {
                blockState = worn;
            }
        }
        return false;
    }
    protected void addAdditionalSaveData(ValueOutput output) {
    }
    protected void readAdditionalSaveData(ValueInput input) {
    }
    public void setHurtsEntities(float damagePerDistance, int damageMax) {
        this.hurtEntities = true;
        this.fallDamagePerDistance = damagePerDistance;
        this.fallDamageMax = damageMax;
    }
    public void disableDrop() {
        this.cancelDrop = true;
    }
    public boolean displayFireAnimation() {
        return false;
    }
    public void fillCrashReportCategory(CrashReportCategory category) {
        super.fillCrashReportCategory(category);
        category.setDetail("Immitating BlockState", blockState.toString());
    }
    public BlockState getBlockState() {
        return blockState;
    }
    protected Component getTypeName() {
        return Component.translatable("entity.minecraft.falling_block_type", blockState.getBlock().getName());
    }
    public Packet<ClientGamePacketListener> getAddEntityPacket(ServerEntity serverEntity) {
        return new ClientboundAddEntityPacket(this, serverEntity, Block.getId(getBlockState()));
    }
    public void recreateFromPacket(ClientboundAddEntityPacket packet) {
        super.recreateFromPacket(packet);
    }
    public Entity teleport(TeleportTransition transition) {
        boolean endTrip = (level().dimension() == Level.END || transition.newLevel().dimension() == Level.END)
            && level().dimension() != transition.newLevel().dimension();
        Entity moved = super.teleport(transition);
        this.forceTickAfterTeleportToDuplicate = moved != null && endTrip;
        return moved;


    }
    public MoveSimulationType getMoveSimulationType() {
        return super.getMoveSimulationType();
    }
}

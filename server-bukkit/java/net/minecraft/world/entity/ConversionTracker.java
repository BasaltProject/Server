// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity;
import java.util.function.BiConsumer;
import java.util.function.BooleanSupplier;
import java.util.function.Supplier;
import net.minecraft.network.syncher.EntityDataAccessor;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.level.storage.ValueInput;
import net.minecraft.world.level.storage.ValueOutput;
public class ConversionTracker<M extends Mob> {
    private final M mob;
    private final EntityDataAccessor<Boolean> conversionId;
    private final Supplier<EntityType<? extends M>> convertsTo;
    private final Supplier<Integer> conversionSound;
    private final BooleanSupplier isAfflicted;
    private final String afflictionTimeTag;
    private final int totalAfflictionTime;
    private final String conversionTimeTag;
    private final int totalConversionTime;
    private final BiConsumer<M, ServerLevel> postConversionStep;
    private int afflictionTime;
    private int conversionTime;
    public ConversionTracker(M mob, EntityDataAccessor<Boolean> inConversionId, Supplier<EntityType<? extends M>> inConvertsTo, Supplier< Integer> inConversionSound, BooleanSupplier inIsAfflicted, String inAfflictionTimeTag, int inTotalAfflictionTime, String inConversionTimeTag, int inTotalConversionTime, BiConsumer<M, ServerLevel> inPostConversionStep) {
        this.mob = mob;
        this.conversionId = inConversionId;
        this.convertsTo = inConvertsTo;
        this.conversionSound = inConversionSound;
        this.isAfflicted = inIsAfflicted;
        this.afflictionTimeTag = inAfflictionTimeTag;
        this.totalAfflictionTime = inTotalAfflictionTime;
        this.conversionTimeTag = inConversionTimeTag;
        this.totalConversionTime = inTotalConversionTime;
        this.postConversionStep = inPostConversionStep;
    }
    private void setConverting(boolean converting) {
        mob.getEntityData().set(conversionId, converting);
    }
    private void doConversion(ServerLevel serverLevel) {
        mob.convertTo(convertsTo.get(), ConversionParams.single(mob, true, true), converted -> {
            if (postConversionStep != null) {
                postConversionStep.accept(converted, serverLevel);
            }
            if (!converted.isSilent()) {
                serverLevel.levelEvent(null, conversionSound.get(), converted.blockPosition(), 0);
            }
        });
    }
    public void tick() {
        if (!(mob.level() instanceof ServerLevel serverLevel) || !mob.isAlive() || mob.isNoAi()) {
            return;
        }
        if (!isAfflicted.getAsBoolean()) {
            afflictionTime = -1;
            setConverting(false);
        } else if (isConverting()) {
            if (--conversionTime < 0) {
                doConversion(serverLevel);
            }
        } else if (++afflictionTime >= totalAfflictionTime) {
            startConversion(totalConversionTime);
        }
    }
    public boolean isConverting() {
        return mob.getEntityData().get(conversionId);
    }
    public void addAdditionalSaveData(ValueOutput output) {
        output.putInt(conversionTimeTag, isConverting() ? conversionTime : -1);
        output.putInt(afflictionTimeTag, isAfflicted.getAsBoolean() ? afflictionTime : -1);
    }
    public void readAdditionalSaveData(ValueInput input) {
        this.afflictionTime = input.getIntOr(afflictionTimeTag, 0);
        int saved = input.getIntOr(conversionTimeTag, -1);
        if (saved == -1) {
            setConverting(false);
        } else {
            startConversion(saved);
        }
    }
    public void startConversion(int time) {
        this.conversionTime = time;
        setConverting(true);
    }
    public void setAfflictionTime(int afflictionTime) {
        this.afflictionTime = afflictionTime;
    }
    public void setConversionTime(int conversionTime) {
        this.conversionTime = conversionTime;
    }
}

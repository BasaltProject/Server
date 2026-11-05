// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.boss.enderdragon.phases;
import java.lang.reflect.Constructor;
import java.util.ArrayList;
import java.util.List;
import net.minecraft.world.entity.boss.enderdragon.EnderDragon;
public class EnderDragonPhase<T extends DragonPhaseInstance> {
    private static final List<EnderDragonPhase<?>> kPhases = new ArrayList<>();
    public static final EnderDragonPhase<DragonHoldingPatternPhase> HOLDING_PATTERN = create(DragonHoldingPatternPhase.class, "HoldingPattern");
    public static final EnderDragonPhase<DragonStrafePlayerPhase> STRAFE_PLAYER = create(DragonStrafePlayerPhase.class, "StrafePlayer");
    public static final EnderDragonPhase<DragonLandingApproachPhase> LANDING_APPROACH = create(DragonLandingApproachPhase.class, "LandingApproach");
    public static final EnderDragonPhase<DragonLandingPhase> LANDING = create(DragonLandingPhase.class, "Landing");
    public static final EnderDragonPhase<DragonTakeoffPhase> TAKEOFF = create(DragonTakeoffPhase.class, "Takeoff");
    public static final EnderDragonPhase<DragonSittingFlamingPhase> SITTING_FLAMING = create(DragonSittingFlamingPhase.class, "SittingFlaming");
    public static final EnderDragonPhase<DragonSittingScanningPhase> SITTING_SCANNING = create(DragonSittingScanningPhase.class, "SittingScanning");
    public static final EnderDragonPhase<DragonSittingAttackingPhase> SITTING_ATTACKING = create(DragonSittingAttackingPhase.class, "SittingAttacking");
    public static final EnderDragonPhase<DragonChargePlayerPhase> CHARGING_PLAYER = create(DragonChargePlayerPhase.class, "ChargingPlayer");
    public static final EnderDragonPhase<DragonDeathPhase> DYING = create(DragonDeathPhase.class, "Dying");
    public static final EnderDragonPhase<DragonHoverPhase> HOVERING = create(DragonHoverPhase.class, "Hover");
    private final int id;
    private final Class<? extends DragonPhaseInstance> instanceClass;
    private final String name;
    private EnderDragonPhase(int id, Class<? extends DragonPhaseInstance> instanceClass, String name) {
        this.id = id;
        this.instanceClass = instanceClass;
        this.name = name;
    }
    private static <P extends DragonPhaseInstance> EnderDragonPhase<P> create(Class<P> type, String name) {
        EnderDragonPhase<P> phase = new EnderDragonPhase<>(kPhases.size(), type, name);
        kPhases.add(phase);
        return phase;
    }
    public DragonPhaseInstance createInstance(EnderDragon dragon) {
        try {
            return getConstructor().newInstance(dragon);
        } catch (Exception failure) {
            throw new Error(failure);
        }
    }
    protected Constructor<? extends DragonPhaseInstance> getConstructor() throws NoSuchMethodException {
        return instanceClass.getConstructor(EnderDragon.class);
    }
    public int getId() {
        return id;
    }
    public String toString() {
        return name + " (#" + id + ")";
    }
    public static EnderDragonPhase<?> getById(int id) {
        return id >= 0 && id < kPhases.size() ? kPhases.get(id) : HOVERING;
    }
    public static int getCount() {
        return kPhases.size();
    }
}

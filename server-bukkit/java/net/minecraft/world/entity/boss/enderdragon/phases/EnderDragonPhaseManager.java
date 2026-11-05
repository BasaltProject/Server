// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.boss.enderdragon.phases;
import net.minecraft.world.entity.boss.enderdragon.EnderDragon;
public class EnderDragonPhaseManager {
    private final EnderDragon dragon;
    private final DragonPhaseInstance[] instances = new DragonPhaseInstance[EnderDragonPhase.getCount()];
    public EnderDragonPhaseManager(EnderDragon dragon) {
        this.dragon = dragon;
    }
    public void setPhase(EnderDragonPhase<?> target) {
        if (dragon.live() instanceof org.bukkit.entity.EnderDragon api) {
            api.setPhase(org.bukkit.entity.EnderDragon.Phase.values()[target.getId()]);
        }
    }
    public DragonPhaseInstance getCurrentPhase() {
        int id = dragon.live() instanceof org.bukkit.entity.EnderDragon api ? api.getPhase().ordinal() : EnderDragonPhase.HOVERING.getId();
        return getPhase(EnderDragonPhase.getById(id));
    }
    @SuppressWarnings("unchecked")
    public <T extends DragonPhaseInstance> T getPhase(EnderDragonPhase<T> phase) {
        int id = phase.getId();
        if (instances[id] == null) {
            instances[id] = phase.createInstance(dragon);
        }
        return (T) instances[id];
    }
}

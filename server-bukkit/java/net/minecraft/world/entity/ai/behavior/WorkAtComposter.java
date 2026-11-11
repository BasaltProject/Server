// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.ai.behavior;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.entity.npc.villager.Villager;
public class WorkAtComposter extends WorkAtPoi {
    protected void useWorkstation(ServerLevel level, Villager body) {
        return;
    }
}

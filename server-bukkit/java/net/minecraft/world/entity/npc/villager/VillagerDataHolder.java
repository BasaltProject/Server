// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.npc.villager;
import net.minecraft.core.BlockPos;
import net.minecraft.world.level.ServerLevelAccessor;
public interface VillagerDataHolder {
    VillagerData getVillagerData();
    void setVillagerData(VillagerData data);
    boolean getVillagerDataFinalized();
    void setVillagerDataFinalized(boolean villagerDataFinalized);
    default void finalizeVillagerType(ServerLevelAccessor level, BlockPos pos) {
        if (!getVillagerDataFinalized()) {
            setVillagerData(getVillagerData().withType(level.registryAccess(), VillagerType.byBiome(level.getBiome(pos))));
            setVillagerDataFinalized(true);
        }
    }
}

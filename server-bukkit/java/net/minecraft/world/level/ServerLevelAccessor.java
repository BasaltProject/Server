// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level;

import net.minecraft.core.BlockPos;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.DifficultyInstance;
import net.minecraft.world.entity.Entity;
public interface ServerLevelAccessor extends LevelAccessor {
    default ServerLevel getLevel() {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.ServerLevelAccessor#getLevel");
    }
    default DifficultyInstance getCurrentDifficultyAt(BlockPos pos) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.ServerLevelAccessor#getCurrentDifficultyAt");
    }
    default void addFreshEntityWithPassengers(Entity entity) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.ServerLevelAccessor#addFreshEntityWithPassengers");
    }
}

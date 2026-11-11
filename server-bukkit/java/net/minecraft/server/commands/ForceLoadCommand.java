// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.commands;
import com.mojang.brigadier.CommandDispatcher;
import net.minecraft.commands.CommandSourceStack;
import net.minecraft.resources.ResourceKey;
import net.minecraft.world.level.ChunkPos;
import net.minecraft.world.level.Level;
public class ForceLoadCommand {
    public static void register(CommandDispatcher<CommandSourceStack> dispatcher) {
        org.basaltmc.bukkit.facade.NativeCommands.delegate(dispatcher, "forceload");
    }
    static record ForceLoadParams(ResourceKey<Level> dimension, int minChunkX, int minChunkZ, int maxChunkX, int maxChunkZ) {
        public ChunkPos min() {
            return new ChunkPos(minChunkX, minChunkZ);
        }
        public ChunkPos max() {
            return new ChunkPos(maxChunkX, maxChunkZ);
        }
    }
}

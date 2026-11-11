// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.commands;
import net.minecraft.network.chat.Component;
import com.mojang.brigadier.CommandDispatcher;
import com.mojang.brigadier.exceptions.CommandSyntaxException;
import net.minecraft.commands.CommandBuildContext;
import net.minecraft.commands.CommandSourceStack;
import net.minecraft.core.Holder;
import net.minecraft.nbt.CompoundTag;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.entity.EntityType;
import net.minecraft.world.phys.Vec3;
public class SummonCommand {
    public static void register(CommandDispatcher<CommandSourceStack> dispatcher, CommandBuildContext context) {
        org.basaltmc.bukkit.facade.NativeCommands.delegate(dispatcher, "summon");
    }
    public static Entity createEntity(CommandSourceStack source, Holder.Reference<EntityType<?>> type, Vec3 pos, CompoundTag nbt, boolean finalize) throws CommandSyntaxException {
        net.minecraft.core.BlockPos at = net.minecraft.core.BlockPos.containing(pos);
        if (!net.minecraft.world.level.Level.isInSpawnableBounds(at)) {
            throw new com.mojang.brigadier.exceptions.SimpleCommandExceptionType(Component.translatable("commands.summon.invalidPosition")).create();
        }
        net.minecraft.server.level.ServerLevel level = source.getLevel();
        if (level.getDifficulty() == net.minecraft.world.Difficulty.PEACEFUL && !type.value().isAllowedInPeaceful()) {
            throw new com.mojang.brigadier.exceptions.SimpleCommandExceptionType(Component.translatable("commands.summon.failed.peaceful")).create();
        }
        CompoundTag tag = nbt.copy();
        tag.putString("id", type.key().identifier().toString());
        Entity entity = net.minecraft.world.entity.EntityType.loadEntityRecursive(tag, level,
            new net.minecraft.world.entity.EntitySpawnRequest(net.minecraft.world.entity.EntitySpawnReason.COMMAND, false), loaded -> {
                loaded.snapTo(pos.x, pos.y, pos.z, loaded.getYRot(), loaded.getXRot());
                return loaded;
            });
        if (entity == null) {
            throw new com.mojang.brigadier.exceptions.SimpleCommandExceptionType(Component.translatable("commands.summon.failed")).create();
        }
        if (finalize && entity instanceof net.minecraft.world.entity.Mob mob) {
            mob.finalizeSpawn(level, level.getCurrentDifficultyAt(entity.blockPosition()), net.minecraft.world.entity.EntitySpawnReason.COMMAND, null);
        }
        if (!level.tryAddFreshEntityWithPassengers(entity)) {
            throw new com.mojang.brigadier.exceptions.SimpleCommandExceptionType(Component.translatable("commands.summon.failed.uuid")).create();
        }
        return entity;
    }
}

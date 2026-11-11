// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.commands;
import com.mojang.brigadier.CommandDispatcher;
import com.mojang.brigadier.exceptions.CommandSyntaxException;
import com.mojang.brigadier.exceptions.SimpleCommandExceptionType;
import com.mojang.datafixers.util.Either;
import java.util.function.Consumer;
import java.util.function.Predicate;
import java.util.function.Supplier;
import net.minecraft.commands.CommandBuildContext;
import net.minecraft.commands.CommandSourceStack;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Holder;
import net.minecraft.network.chat.Component;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.level.biome.Biome;
public class FillBiomeCommand {
    public static final SimpleCommandExceptionType ERROR_NOT_LOADED = new SimpleCommandExceptionType(Component.translatable("argument.pos.unloaded"));
    public static final SimpleCommandExceptionType ERROR_NO_BIOMES_SET = new SimpleCommandExceptionType(Component.translatable("commands.fillbiome.failed.no_biomes_set"));
    public static void register(CommandDispatcher<CommandSourceStack> dispatcher, CommandBuildContext context) {
        org.basaltmc.bukkit.facade.NativeCommands.delegate(dispatcher, "fillbiome");
    }
    public static Either<Integer, CommandSyntaxException> fill(ServerLevel level, BlockPos rawFrom, BlockPos rawTo, Holder<Biome> biome) {
        return fill(level, rawFrom, rawTo, biome, any -> true, messages -> {});
    }
    public static Either<Integer, CommandSyntaxException> fill(ServerLevel level, BlockPos rawFrom, BlockPos rawTo, Holder<Biome> biome, Predicate<Holder<Biome>> filter, Consumer<Supplier<Component>> successMessageConsumer) {
        org.bukkit.block.Biome target = biome.unwrapKey().map(key -> org.bukkit.Registry.BIOME.get(org.bukkit.NamespacedKey.fromString(key.identifier().toString()))).orElse(null);
        if (target == null) {
            return Either.right(ERROR_NO_BIOMES_SET.create());
        }
        BlockPos from = new BlockPos(Math.min(rawFrom.getX(), rawTo.getX()) & ~3, Math.min(rawFrom.getY(), rawTo.getY()) & ~3, Math.min(rawFrom.getZ(), rawTo.getZ()) & ~3);
        BlockPos to = new BlockPos(Math.max(rawFrom.getX(), rawTo.getX()) | 3, Math.max(rawFrom.getY(), rawTo.getY()) | 3, Math.max(rawFrom.getZ(), rawTo.getZ()) | 3);
        org.bukkit.World world = level.getWorld();
        int changed = 0;
        for (int x = from.getX(); x <= to.getX(); x += 4) {
            for (int y = from.getY(); y <= to.getY(); y += 4) {
                for (int z = from.getZ(); z <= to.getZ(); z += 4) {
                    if (!world.isChunkLoaded(x >> 4, z >> 4)) {
                        return Either.right(ERROR_NOT_LOADED.create());
                    }
                    if (filter.test(level.getBiome(new BlockPos(x, y, z))) && world.getBiome(x, y, z) != target) {
                        world.setBiome(x, y, z, target);
                        changed++;
                    }
                }
            }
        }
        if (changed == 0) {
            return Either.right(ERROR_NO_BIOMES_SET.create());
        }
        int count = changed;
        successMessageConsumer.accept(() -> Component.translatable("commands.fillbiome.success.count", count, from.getX(), from.getY(), from.getZ(), to.getX(), to.getY(), to.getZ()));
        return Either.left(changed);
    }
}

// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.timers;
import com.mojang.serialization.MapCodec;
import net.minecraft.resources.Identifier;
import net.minecraft.server.MinecraftServer;
import com.mojang.serialization.codecs.RecordCodecBuilder;
import net.minecraft.server.ServerFunctionManager;
import net.minecraft.commands.CommandSourceStack;
import net.minecraft.commands.functions.CommandFunction;
public record FunctionTagCallback(Identifier tagId) implements TimerCallback<MinecraftServer> {
    public static final MapCodec<FunctionTagCallback> CODEC = RecordCodecBuilder.mapCodec(i -> i.group(
        Identifier.CODEC.fieldOf("id").forGetter(FunctionTagCallback::tagId)).apply(i, FunctionTagCallback::new));
    @Override
    public void handle(MinecraftServer server, TimerQueue<MinecraftServer> queue, long time) {
        ServerFunctionManager manager = server.getFunctions();
        for (CommandFunction<CommandSourceStack> function : manager.getTag(tagId)) {
            manager.execute(function, manager.getGameLoopSender());
        }
    }
    @Override
    public MapCodec<FunctionTagCallback> codec() {
        return CODEC;
    }
}

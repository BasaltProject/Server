// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.commands;
import com.mojang.brigadier.exceptions.SimpleCommandExceptionType;
import net.minecraft.util.Mth;
import com.mojang.brigadier.CommandDispatcher;
import java.util.function.BiConsumer;
import java.util.function.BiPredicate;
import java.util.function.ToIntFunction;
import net.minecraft.commands.CommandSourceStack;
import net.minecraft.network.chat.Component;
import net.minecraft.server.level.ServerPlayer;
public class ExperienceCommand {
    public static void register(CommandDispatcher<CommandSourceStack> dispatcher) {
        org.basaltmc.bukkit.facade.NativeCommands.delegate(dispatcher, "experience", "xp");
    }
    static enum Type {
        POINTS("points", (player, amount) -> player.giveExperiencePoints(amount), (player, amount) -> {
            if (amount >= player.getXpNeededForNextLevel()) {
                return false;
            }
            player.setExperiencePoints(amount);
            return true;
        }, player -> Mth.floor(player.experienceProgress * player.getXpNeededForNextLevel())),
        LEVELS("levels", (player, amount) -> player.giveExperienceLevels(amount), (player, amount) -> {
            player.setExperienceLevels(amount);
            return true;
        }, player -> player.experienceLevel);

        public final BiConsumer<ServerPlayer, Integer> add;
        public final BiPredicate<ServerPlayer, Integer> set;
        public final ToIntFunction<ServerPlayer> query;
        public final String queryTranslationKey;
        public final CommandResponseTracker.MessagesWithArg<ServerPlayer, Integer> addResponse;
        public final CommandResponseTracker.MessagesWithArg<ServerPlayer, Integer> setResponse;

        Type(String name, BiConsumer<ServerPlayer, Integer> add, BiPredicate<ServerPlayer, Integer> set, ToIntFunction<ServerPlayer> query) {
            this.add = add;
            this.set = set;
            this.query = query;
            this.queryTranslationKey = "commands.experience.query." + name;
            this.addResponse = CommandResponseTracker.messages(
                (player, total, amount) -> Component.translatable("commands.experience.add." + name + ".success.single", amount, player.getDisplayName()),
                (count, total, amount) -> Component.translatable("commands.experience.add." + name + ".success.multiple", amount, count));
            this.setResponse = CommandResponseTracker.messages(new SimpleCommandExceptionType(Component.translatable("commands.experience.set.points.invalid")),
                (player, total, amount) -> Component.translatable("commands.experience.set." + name + ".success.single", amount, player.getDisplayName()),
                (count, total, amount) -> Component.translatable("commands.experience.set." + name + ".success.multiple", amount, count));
        }

        public Component queryResponse(ServerPlayer target, int result) {
            return Component.translatable(queryTranslationKey, target.getDisplayName(), result);
        }
    }
}

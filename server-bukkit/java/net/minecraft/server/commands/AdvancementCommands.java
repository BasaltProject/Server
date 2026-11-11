// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.commands;
import net.minecraft.network.chat.Component;
import com.mojang.brigadier.exceptions.Dynamic2CommandExceptionType;
import com.mojang.brigadier.exceptions.Dynamic3CommandExceptionType;
import net.minecraft.advancements.Advancement;
import com.mojang.brigadier.CommandDispatcher;
import com.mojang.brigadier.exceptions.CommandSyntaxException;
import net.minecraft.advancements.AdvancementHolder;
import net.minecraft.commands.CommandSourceStack;
import net.minecraft.server.level.ServerPlayer;
public class AdvancementCommands {
    public static void register(CommandDispatcher<CommandSourceStack> dispatcher) {
        org.basaltmc.bukkit.facade.NativeCommands.delegate(dispatcher, "advancement");
    }
    static enum Action {
        GRANT("grant"), REVOKE("revoke");

        public final CommandResponseTracker.MessagesWithArg<ServerPlayer, AdvancementHolder> singleAdvancementSuccessResponse;
        public final CommandResponseTracker.DispatchWithArg<CommandSyntaxException, ServerPlayer, AdvancementHolder> singleAdvancementsError;
        public final CommandResponseTracker.MessagesWithArg<ServerPlayer, Integer> multipleAdvancementsSuccessResponse;
        public final CommandResponseTracker.DispatchWithArg<CommandSyntaxException, ServerPlayer, Integer> multipleAdvancementsError;
        public final CommandResponseTracker.MessagesWithArgs<ServerPlayer, AdvancementHolder, String> criterionSuccessResponse;
        public final CommandResponseTracker.DispatchWithArgs<CommandSyntaxException, ServerPlayer, AdvancementHolder, String> criterionError;

        Action(String key) {
            String base = "commands.advancement." + key + ".";
            singleAdvancementSuccessResponse = CommandResponseTracker.messages(
                (player, total, advancement) -> Component.translatable(base + "one.to.one.success", Advancement.name(advancement), player.getDisplayName()),
                (count, total, advancement) -> Component.translatable(base + "one.to.many.success", Advancement.name(advancement), count));
            Dynamic2CommandExceptionType oneToOne = new Dynamic2CommandExceptionType((advancement, player) -> Component.translatableEscape(base + "one.to.one.failure", advancement, player));
            Dynamic2CommandExceptionType oneToMany = new Dynamic2CommandExceptionType((advancement, count) -> Component.translatableEscape(base + "one.to.many.failure", advancement, count));
            singleAdvancementsError = new CommandResponseTracker.DispatchWithArg<>(
                (player, total, advancement) -> oneToOne.create(Advancement.name(advancement), player.getDisplayName()),
                (count, total, advancement) -> oneToMany.create(Advancement.name(advancement), count));
            multipleAdvancementsSuccessResponse = CommandResponseTracker.messages(
                (player, total, amount) -> Component.translatable(base + "many.to.one.success", amount, player.getDisplayName()),
                (count, total, amount) -> Component.translatable(base + "many.to.many.success", amount, count));
            Dynamic2CommandExceptionType manyToOne = new Dynamic2CommandExceptionType((amount, player) -> Component.translatableEscape(base + "many.to.one.failure", amount, player));
            Dynamic2CommandExceptionType manyToMany = new Dynamic2CommandExceptionType((amount, count) -> Component.translatableEscape(base + "many.to.many.failure", amount, count));
            multipleAdvancementsError = new CommandResponseTracker.DispatchWithArg<>(
                (player, total, amount) -> manyToOne.create(amount, player.getDisplayName()),
                (count, total, amount) -> manyToMany.create(amount, count));
            criterionSuccessResponse = CommandResponseTracker.messages(
                (player, total, advancement, criterion) -> Component.translatable(base + "criterion.to.one.success", criterion, Advancement.name(advancement), player.getDisplayName()),
                (count, total, advancement, criterion) -> Component.translatable(base + "criterion.to.many.success", criterion, Advancement.name(advancement), count));
            Dynamic3CommandExceptionType criterionOne = new Dynamic3CommandExceptionType((criterion, advancement, player) -> Component.translatableEscape(base + "criterion.to.one.failure", criterion, advancement, player));
            Dynamic3CommandExceptionType criterionMany = new Dynamic3CommandExceptionType((criterion, advancement, count) -> Component.translatableEscape(base + "criterion.to.many.failure", criterion, advancement, count));
            criterionError = new CommandResponseTracker.DispatchWithArgs<>(
                (player, total, advancement, criterion) -> criterionOne.create(criterion, Advancement.name(advancement), player.getDisplayName()),
                (count, total, advancement, criterion) -> criterionMany.create(criterion, Advancement.name(advancement), count));
        }

        public int perform(ServerPlayer player, Iterable<AdvancementHolder> advancements, boolean showAdvancements) {
            int count = 0;
            if (!showAdvancements) {
                player.getAdvancements().flushDirty(player, true);
            }
            for (AdvancementHolder advancement : advancements) {
                if (perform(player, advancement)) {
                    count++;
                }
            }
            if (!showAdvancements) {
                player.getAdvancements().flushDirty(player, false);
            }
            return count;
        }
        protected boolean perform(ServerPlayer player, AdvancementHolder advancement) {
            var progress = player.getAdvancements().getOrStartProgress(advancement);
            if (this == GRANT) {
                if (progress.isDone()) {
                    return false;
                }
                for (String criterion : progress.getRemainingCriteria()) {
                    player.getAdvancements().award(advancement, criterion);
                }
                return true;
            }
            if (!progress.hasProgress()) {
                return false;
            }
            for (String criterion : progress.getCompletedCriteria()) {
                player.getAdvancements().revoke(advancement, criterion);
            }
            return true;
        }
        protected boolean performCriterion(ServerPlayer player, AdvancementHolder advancement, String criterion) {
            return this == GRANT ? player.getAdvancements().award(advancement, criterion) : player.getAdvancements().revoke(advancement, criterion);
        }
    }
    static enum Mode {
        ONLY, THROUGH, FROM, UNTIL, EVERYTHING;
    }
}

// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.advancements;
import it.unimi.dsi.fastutil.objects.ObjectArrayList;
import java.util.function.Predicate;
import net.minecraft.advancements.Advancement;
import net.minecraft.advancements.AdvancementNode;
import net.minecraft.advancements.DisplayInfo;
public class AdvancementVisibilityEvaluator {
    private static final int kDepth = 2;
    private static VisibilityRule ruleFor(Advancement advancement, boolean done) {
        DisplayInfo display = advancement.display().orElse(null);
        if (display == null) {
            return VisibilityRule.HIDE;
        }
        if (done) {
            return VisibilityRule.SHOW;
        }
        return display.hidden() ? VisibilityRule.HIDE : VisibilityRule.NO_CHANGE;
    }
    private static boolean unfinishedVisible(ObjectArrayList<VisibilityRule> ascendants) {
        for (int i = 0; i <= kDepth; i++) {
            VisibilityRule rule = ascendants.peek(i);
            if (rule != VisibilityRule.NO_CHANGE) {
                return rule == VisibilityRule.SHOW;
            }
        }
        return false;
    }
    private static boolean evaluate(AdvancementNode node, ObjectArrayList<VisibilityRule> ascendants, Predicate<AdvancementNode> isDone, Output output) {
        boolean selfDone = isDone.test(node);
        boolean anyDone = selfDone;
        ascendants.push(ruleFor(node.advancement(), selfDone));
        for (AdvancementNode child : node.children()) {
            anyDone |= evaluate(child, ascendants, isDone, output);
        }
        boolean visible = anyDone || unfinishedVisible(ascendants);
        ascendants.pop();
        output.accept(node, visible);
        return anyDone;
    }
    public static void evaluateVisibility(AdvancementNode node, Predicate<AdvancementNode> isDone, Output output) {
        ObjectArrayList<VisibilityRule> stack = new ObjectArrayList<>();
        for (int i = 0; i <= kDepth; i++) {
            stack.push(VisibilityRule.NO_CHANGE);
        }
        evaluate(node.root(), stack, isDone, output);
    }
    private static enum VisibilityRule {
        SHOW,
        HIDE,
        NO_CHANGE
    }
    @FunctionalInterface
    public interface Output {
        void accept(AdvancementNode advancement, boolean visible);
    }
}

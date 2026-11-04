// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.player;
import it.unimi.dsi.fastutil.objects.Reference2IntMap;
import it.unimi.dsi.fastutil.objects.Reference2IntOpenHashMap;
import java.util.List;
public class StackedContents<T> {
    public final Reference2IntOpenHashMap<T> amounts = new Reference2IntOpenHashMap<>();
    public boolean tryPick(List<? extends IngredientInfo<T>> ingredients, int amount, Output<T> output) {
        int[] chosen = assign(ingredients, amount);
        if (chosen == null) {
            return false;
        }
        if (output != null) {
            for (int index : chosen) {
                output.accept(candidates.get(index));
            }
        }
        return true;
    }
    public int tryPickAll(List<? extends IngredientInfo<T>> ingredients, int maxSize, Output<T> output) {
        int low = 0;
        int high = Math.max(maxSize, 0);
        while (low < high) {
            int mid = (low + high + 1) >>> 1;
            if (assign(ingredients, mid) != null) {
                low = mid;
            } else {
                high = mid - 1;
            }
        }
        if (low > 0 && output != null) {
            tryPick(ingredients, low, output);
        }
        return low;
    }
    public void clear() {
        amounts.clear();
    }
    public void account(T item, int count) {
        if (count > 0) {
            amounts.mergeInt(item, count, Integer::sum);
        }
    }
    public int getResultUpperBound(List<? extends IngredientInfo<T>> ingredients) {
        if (ingredients.isEmpty()) {
            return 0;
        }
        int pooled = 0;
        int bound = Integer.MAX_VALUE;
        for (IngredientInfo<T> ingredient : ingredients) {
            int available = 0;
            for (Reference2IntMap.Entry<T> entry : amounts.reference2IntEntrySet()) {
                if (ingredient.acceptsItem(entry.getKey())) {
                    available += entry.getIntValue();
                }
            }
            bound = Math.min(bound, available);
        }
        for (Reference2IntMap.Entry<T> entry : amounts.reference2IntEntrySet()) {
            for (IngredientInfo<T> ingredient : ingredients) {
                if (ingredient.acceptsItem(entry.getKey())) {
                    pooled += entry.getIntValue();
                    break;
                }
            }
        }
        return Math.min(bound, pooled / ingredients.size());
    }
    class RecipePicker {
        public RecipePicker(List<? extends IngredientInfo<T>> ingredients) {
            
        }
        public boolean tryPick(int capacity, Output<T> output) {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.entity.player.StackedContents.RecipePicker#tryPick");
        }
        public int tryPickAll(int maxSize, Output<T> output) {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.entity.player.StackedContents.RecipePicker#tryPickAll");
        }
    }
    public interface IngredientInfo<T> {
        boolean acceptsItem(T item);
    }
    public interface Output<T> {
        void accept(T item);
    }
    private final java.util.List<T> candidates = new java.util.ArrayList<>();
    private int[] assign(List<? extends IngredientInfo<T>> ingredients, int amount) {
        candidates.clear();
        candidates.addAll(amounts.keySet());
        int[] left = new int[candidates.size()];
        for (int i = 0; i < left.length; i++) {
            left[i] = amounts.getInt(candidates.get(i));
        }
        int[] chosen = new int[ingredients.size()];
        return place(ingredients, amount, 0, left, chosen) ? chosen : null;
    }
    private boolean place(List<? extends IngredientInfo<T>> ingredients, int amount, int at, int[] left, int[] chosen) {
        if (at == ingredients.size()) {
            return true;
        }
        for (int i = 0; i < left.length; i++) {
            if (left[i] >= amount && ingredients.get(at).acceptsItem(candidates.get(i))) {
                left[i] -= amount;
                chosen[at] = i;
                if (place(ingredients, amount, at + 1, left, chosen)) {
                    return true;
                }
                left[i] += amount;
            }
        }
        return false;
    }
}

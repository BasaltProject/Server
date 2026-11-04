// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.item.crafting;
import com.mojang.serialization.MapCodec;
import java.util.List;
import java.util.Map;
import java.util.Optional;
import net.minecraft.network.RegistryFriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
public final class ShapedRecipePattern {
    public static final char EMPTY_SLOT = 0;
    public static final MapCodec<ShapedRecipePattern> MAP_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.item.crafting.ShapedRecipePattern.class, "MAP_CODEC");
    public static final StreamCodec<RegistryFriendlyByteBuf, ShapedRecipePattern> STREAM_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.streamCodec(net.minecraft.world.item.crafting.ShapedRecipePattern.class, "STREAM_CODEC");
    private final int width;
    private final int height;
    private final List<Optional<Ingredient>> ingredients;
    private final Optional<Data> data;

    public ShapedRecipePattern(int width, int height, List<Optional<Ingredient>> ingredients, Optional<Data> data) {
        this.width = width;
        this.height = height;
        this.ingredients = ingredients;
        this.data = data;
    }
    public static ShapedRecipePattern of(Map<Character, Ingredient> key, String... pattern) {
        return of(key, java.util.Arrays.asList(pattern));
    }
    public static ShapedRecipePattern of(Map<Character, Ingredient> key, List<String> pattern) {
        String[] rows = shrink(pattern);
        int rowCount = rows.length;
        int columnCount = rowCount == 0 ? 0 : rows[0].length();
        List<Optional<Ingredient>> cells = new java.util.ArrayList<>(rowCount * columnCount);
        for (String row : rows) {
            for (int x = 0; x < row.length(); x++) {
                char symbol = row.charAt(x);
                cells.add(symbol == ' ' ? Optional.empty() : Optional.ofNullable(key.get(symbol)));
            }
        }
        return new ShapedRecipePattern(columnCount, rowCount, cells, Optional.of(new Data(key, pattern)));
    }
    static String[] shrink(List<String> pattern) {
        int top = Integer.MAX_VALUE;
        int bottom = 0;
        int left = Integer.MAX_VALUE;
        int right = 0;
        for (int y = 0; y < pattern.size(); y++) {
            String row = pattern.get(y);
            int first = 0;
            while (first < row.length() && row.charAt(first) == ' ') {
                first++;
            }
            int last = row.length() - 1;
            while (last >= 0 && row.charAt(last) == ' ') {
                last--;
            }
            if (first <= last) {
                top = Math.min(top, y);
                bottom = y;
                left = Math.min(left, first);
                right = Math.max(right, last);
            }
        }
        if (top == Integer.MAX_VALUE) {
            return new String[0];
        }
        String[] rows = new String[bottom - top + 1];
        for (int y = 0; y < rows.length; y++) {
            rows[y] = pattern.get(top + y).substring(left, right + 1);
        }
        return rows;
    }
    public boolean matches(CraftingInput input) {
        if (input.width() != width || input.height() != height) {
            return false;
        }
        return matches(input, false) || matches(input, true);
    }
    private boolean matches(CraftingInput input, boolean mirrored) {
        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                Optional<Ingredient> wanted = ingredients.get(mirrored ? width - x - 1 + y * width : x + y * width);
                if (!Ingredient.testOptionalIngredient(wanted, input.getItem(x, y))) {
                    return false;
                }
            }
        }
        return true;
    }
    public int width() {
        return width;
    }
    public int height() {
        return height;
    }
    public List<Optional<Ingredient>> ingredients() {
        return ingredients;
    }
    public static record Data(Map<Character, Ingredient> key, List<String> pattern) {
        public static final MapCodec<Data> MAP_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.item.crafting.ShapedRecipePattern.Data.class, "MAP_CODEC");
    }
}

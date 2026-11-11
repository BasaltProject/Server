// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.block.state.pattern;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.function.Predicate;
public class BlockPatternBuilder {
    private final List<String[]> pattern = new ArrayList<>();
    private final Map<Character, Predicate<BlockInWorld>> lookup = new HashMap<>();
    private final Set<Character> unknown = new HashSet<>();
    private int height;
    private int width;
    private BlockPatternBuilder() {
        lookup.put(' ', blockInWorld -> true);
    }
    public BlockPatternBuilder aisle(String... aisle) {
        if (aisle == null || aisle.length == 0 || aisle[0].isEmpty()) {
            throw new IllegalArgumentException("Empty pattern for aisle");
        }
        if (pattern.isEmpty()) {
            height = aisle.length;
            width = aisle[0].length();
        }
        if (aisle.length != height) {
            throw new IllegalArgumentException("Expected aisle with height of " + height + ", but was given one with a height of " + aisle.length + ")");
        }
        for (String row : aisle) {
            if (row.length() != width) {
                throw new IllegalArgumentException("Not all rows in the given aisle are the correct width (expected " + width + ", found one with " + row.length() + ")");
            }
            for (char c : row.toCharArray()) {
                if (!lookup.containsKey(c)) {
                    unknown.add(c);
                }
            }
        }
        pattern.add(aisle);
        return this;
    }
    public static BlockPatternBuilder start() {
        return new BlockPatternBuilder();
    }
    public BlockPatternBuilder where(char character, Predicate< BlockInWorld> predicate) {
        lookup.put(character, predicate);
        unknown.remove(character);
        return this;
    }
    public BlockPattern build() {
        if (!unknown.isEmpty()) {
            throw new IllegalStateException("Predicates for character(s) " + unknown + " are missing");
        }
        @SuppressWarnings("unchecked")
        Predicate<BlockInWorld>[][][] result = (Predicate<BlockInWorld>[][][]) java.lang.reflect.Array.newInstance(Predicate.class, pattern.size(), height, width);
        for (int aisle = 0; aisle < pattern.size(); aisle++) {
            for (int row = 0; row < height; row++) {
                for (int col = 0; col < width; col++) {
                    result[aisle][row][col] = lookup.get(pattern.get(aisle)[row].charAt(col));
                }
            }
        }
        return new BlockPattern(result);


    }
}

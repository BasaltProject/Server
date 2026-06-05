package org.basaltmc.bukkit;

import java.util.Arrays;
import java.util.HashMap;
import java.util.Locale;
import java.util.Map;
import org.bukkit.Material;

public final class Registries {
    /** The properties of one block in the order its state ids are laid out, first property slowest. */
    public record Properties(String[] names, String[][] values, int[] strides) {
        int indexOf(String name) {
            for (int i = 0; i < names.length; i++) {
                if (names[i].equals(name)) {
                    return i;
                }
            }
            return -1;
        }
    }

    private static final Properties kNone = new Properties(new String[0], new String[0][], new int[0]);

    private static String[] blockNames;
    private static int[] blockFirst;
    private static int[] blockCount;
    private static Map<String, Integer> blockByName;
    private static Material[] materials;
    private static Properties[] properties;
    private static int[] defaultStates;
    private static String[] biomes;
    private static String[] items;

    private Registries() {}

    private static synchronized void loadBlocks() {
        if (blockNames != null) {
            return;
        }
        String[] flat = WorldNatives.blockTable();
        int count = flat.length / 3;
        String[] names = new String[count];
        int[] first = new int[count];
        int[] sizes = new int[count];
        Integer[] order = new Integer[count];
        for (int i = 0; i < count; i++) {
            names[i] = flat[i * 3];
            first[i] = Integer.parseInt(flat[i * 3 + 1]);
            sizes[i] = Integer.parseInt(flat[i * 3 + 2]);
            order[i] = i;
        }
        Arrays.sort(order, (a, b) -> Integer.compare(first[a], first[b]));
        String[] sortedNames = new String[count];
        int[] sortedFirst = new int[count];
        int[] sortedCount = new int[count];
        Map<String, Integer> byName = new HashMap<>();
        for (int i = 0; i < count; i++) {
            sortedNames[i] = names[order[i]];
            sortedFirst[i] = first[order[i]];
            sortedCount[i] = sizes[order[i]];
            byName.put(sortedNames[i], i);
        }
        materials = new Material[count];
        properties = new Properties[count];
        defaultStates = new int[count];
        Arrays.fill(defaultStates, -1);
        blockFirst = sortedFirst;
        blockCount = sortedCount;
        blockByName = byName;
        blockNames = sortedNames;
    }

    public static int blockCount() {
        loadBlocks();
        return blockNames.length;
    }

    public static String blockName(int block) {
        loadBlocks();
        return blockNames[block];
    }

    public static int blockIndex(String id) {
        loadBlocks();
        Integer found = blockByName.get(id);
        return found == null ? -1 : found;
    }

    public static int blockOfState(int state) {
        loadBlocks();
        int low = 0;
        int high = blockFirst.length - 1;
        while (low <= high) {
            int middle = (low + high) >>> 1;
            if (state < blockFirst[middle]) {
                high = middle - 1;
            } else if (state >= blockFirst[middle] + blockCount[middle]) {
                low = middle + 1;
            } else {
                return middle;
            }
        }
        return -1;
    }

    public static int firstState(int block) {
        loadBlocks();
        return blockFirst[block];
    }

    public static int stateCount(int block) {
        loadBlocks();
        return blockCount[block];
    }

    public static synchronized int defaultState(int block) {
        loadBlocks();
        if (defaultStates[block] < 0) {
            defaultStates[block] = WorldNatives.stateId(blockNames[block]);
        }
        return defaultStates[block];
    }

    public static synchronized Material materialOfBlock(int block) {
        loadBlocks();
        Material material = materials[block];
        if (material == null) {
            String id = blockNames[block];
            material = Material.getMaterial(id.substring(id.indexOf(':') + 1).toUpperCase(Locale.ROOT));
            materials[block] = material == null ? Material.AIR : material;
            material = materials[block];
        }
        return material;
    }

    public static Material materialOfState(int state) {
        int block = blockOfState(state);
        return block < 0 ? Material.AIR : materialOfBlock(block);
    }

    public static int defaultStateOf(Material material) {
        int block = blockIndex(material.getKey().toString());
        return block < 0 ? -1 : defaultState(block);
    }

    public static synchronized Properties propertiesOf(int block) {
        loadBlocks();
        Properties found = properties[block];
        if (found == null) {
            String[] flat = WorldNatives.blockProperties(blockNames[block]);
            if (flat.length == 0) {
                found = kNone;
            } else {
                int count = flat.length / 2;
                String[] names = new String[count];
                String[][] values = new String[count][];
                int[] strides = new int[count];
                for (int i = 0; i < count; i++) {
                    names[i] = flat[i * 2];
                    values[i] = flat[i * 2 + 1].split("\\|");
                }
                int stride = 1;
                for (int i = count - 1; i >= 0; i--) {
                    strides[i] = stride;
                    stride *= values[i].length;
                }
                found = new Properties(names, values, strides);
            }
            properties[block] = found;
        }
        return found;
    }

    public static synchronized String[] biomeIds() {
        if (biomes == null) {
            biomes = WorldNatives.biomeIds();
        }
        return biomes;
    }

    public static synchronized String[] itemIds() {
        if (items == null) {
            items = WorldNatives.itemIds();
        }
        return items;
    }
}

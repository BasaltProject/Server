package org.basaltmc.bukkit.item;

import java.util.ArrayList;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import org.bukkit.inventory.meta.components.CustomModelDataComponent;

public final class MetaModelData {
    private MetaModelData() {}

    public static final class ModelData implements CustomModelDataComponent {
        final Map<String, Object> data;

        ModelData(Map<String, Object> data) {
            this.data = data;
        }

        private List<Object> list(String key) {
            List<Object> found = Nbt.list(data.get(key));
            return found == null ? List.of() : found;
        }

        private void store(String key, List<?> values) {
            if (values == null || values.isEmpty()) {
                data.remove(key);
            } else {
                data.put(key, new ArrayList<Object>(values));
            }
        }

        @Override
        public List<Float> getFloats() {
            List<Float> out = new ArrayList<>();
            for (Object value : list("floats")) {
                out.add(((Number) value).floatValue());
            }
            return out;
        }

        @Override
        public void setFloats(List<Float> floats) {
            store("floats", floats);
        }

        @Override
        public List<Boolean> getFlags() {
            List<Boolean> out = new ArrayList<>();
            for (Object value : list("flags")) {
                out.add(((Number) value).intValue() != 0);
            }
            return out;
        }

        @Override
        public void setFlags(List<Boolean> flags) {
            if (flags == null) {
                data.remove("flags");
                return;
            }
            List<Object> bytes = new ArrayList<>();
            for (Boolean flag : flags) {
                bytes.add(Nbt.bool(flag));
            }
            store("flags", bytes);
        }

        @Override
        public List<String> getStrings() {
            List<String> out = new ArrayList<>();
            for (Object value : list("strings")) {
                out.add(String.valueOf(value));
            }
            return out;
        }

        @Override
        public void setStrings(List<String> strings) {
            store("strings", strings);
        }

        @Override
        public List<org.bukkit.Color> getColors() {
            List<org.bukkit.Color> out = new ArrayList<>();
            for (Object value : list("colors")) {
                out.add(org.bukkit.Color.fromRGB(((Number) value).intValue() & 0xFFFFFF));
            }
            return out;
        }

        @Override
        public void setColors(List<org.bukkit.Color> colors) {
            if (colors == null) {
                data.remove("colors");
                return;
            }
            List<Object> ints = new ArrayList<>();
            for (org.bukkit.Color color : colors) {
                ints.add(color.asRGB());
            }
            store("colors", ints);
        }

        @Override
        public Map<String, Object> serialize() {
            Map<String, Object> out = new LinkedHashMap<>();
            out.put("floats", getFloats());
            out.put("flags", getFlags());
            out.put("strings", getStrings());
            return out;
        }
    }
}

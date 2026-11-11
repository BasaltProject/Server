// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.parsing.packrat;

import java.util.ArrayList;
import java.util.List;

public interface ErrorCollector<S> {
    void store(int cursor, SuggestionSupplier<S> suggestions, Object reason);

    default void store(int cursor, Object reason) {
        store(cursor, SuggestionSupplier.empty(), reason);
    }

    void finish(int finalCursor);

    public static class Nop<S> implements ErrorCollector<S> {
        @Override
        public void store(int cursor, SuggestionSupplier<S> suggestions, Object reason) {}

        @Override
        public void finish(int finalCursor) {}
    }

    public static class LongestOnly<S> implements ErrorCollector<S> {
        private final List<ErrorEntry<S>> entries = new ArrayList<>();
        private int cursor = -1;

        @Override
        public void finish(int finalCursor) {
            if (finalCursor > cursor) {
                entries.clear();
                cursor = finalCursor;
            }
        }

        @Override
        public void store(int cursor, SuggestionSupplier<S> suggestions, Object reason) {
            if (cursor < this.cursor) {
                return;
            }
            if (cursor > this.cursor) {
                entries.clear();
                this.cursor = cursor;
            }
            entries.add(new ErrorEntry<>(cursor, suggestions, reason));
        }

        public List<ErrorEntry<S>> entries() {
            return List.copyOf(entries);
        }

        public int cursor() {
            return cursor;
        }
    }
}

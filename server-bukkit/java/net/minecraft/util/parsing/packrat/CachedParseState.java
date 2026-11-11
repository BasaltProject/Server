// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.parsing.packrat;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

public abstract class CachedParseState<S> implements ParseState<S> {
    private final Map<Integer, PositionCache> positions = new HashMap<>();
    private final Scope scope = new Scope();
    private final List<SimpleControl> controls = new ArrayList<>();
    private final ErrorCollector<S> errors;
    private final ErrorCollector<S> nop = new ErrorCollector.Nop<>();
    private final Silent silent = new Silent();
    private ErrorCollector<S> activeErrors;
    private int controlDepth;

    protected CachedParseState(ErrorCollector<S> errorCollector) {
        this.errors = errorCollector;
        this.activeErrors = errorCollector;
    }

    @Override
    public Scope scope() {
        return scope;
    }

    @Override
    public ErrorCollector<S> errorCollector() {
        return activeErrors;
    }

    @Override
    @SuppressWarnings("unchecked")
    public <T> T parse(NamedRule<S, T> rule) {
        int mark = mark();
        PositionCache cache = positions.computeIfAbsent(mark, key -> new PositionCache());
        int keyIndex = cache.findKeyIndex(rule.name());
        if (keyIndex != -1) {
            CacheEntry<T> entry = cache.getValue(keyIndex);
            if (entry != null) {
                if (entry == CacheEntry.NEGATIVE) {
                    return null;
                }
                restore(entry.markAfterParse());
                return entry.value();
            }
        } else {
            keyIndex = cache.allocateNewEntry(rule.name());
        }
        T result = rule.value().parse(this);
        if (result == null) {
            cache.setValue(keyIndex, CacheEntry.negativeEntry());
            restore(mark);
        } else {
            cache.setValue(keyIndex, new CacheEntry<>(result, mark()));
        }
        return result;
    }

    @Override
    public Control acquireControl() {
        if (controlDepth == controls.size()) {
            controls.add(new SimpleControl());
        }
        SimpleControl control = controls.get(controlDepth++);
        control.reset();
        return control;
    }

    @Override
    public void releaseControl() {
        controlDepth--;
    }

    @Override
    public ParseState<S> silent() {
        return silent;
    }

    static class PositionCache {
        public static final int ENTRY_STRIDE = 2;
        private Atom<?>[] keys = new Atom<?>[4];
        private CacheEntry<?>[] values = new CacheEntry<?>[4];
        private int size;

        public int findKeyIndex(Atom<?> key) {
            for (int index = 0; index < size; index++) {
                if (keys[index] == key || keys[index].equals(key)) {
                    return index;
                }
            }
            return -1;
        }

        public int allocateNewEntry(Atom<?> key) {
            if (size == keys.length) {
                keys = Arrays.copyOf(keys, size * 2);
                values = Arrays.copyOf(values, size * 2);
            }
            keys[size] = key;
            return size++;
        }

        @SuppressWarnings("unchecked")
        public <T> CacheEntry<T> getValue(int keyIndex) {
            return (CacheEntry<T>) values[keyIndex];
        }

        public void setValue(int keyIndex, CacheEntry<?> entry) {
            values[keyIndex] = entry;
        }
    }

    static record CacheEntry<T>(T value, int markAfterParse) {
        public static final CacheEntry<?> NEGATIVE = new CacheEntry<>(null, -1);

        @SuppressWarnings("unchecked")
        public static <T> CacheEntry<T> negativeEntry() {
            return (CacheEntry<T>) NEGATIVE;
        }
    }

    class Silent implements ParseState<S> {
        private Silent() {}

        @Override
        public ErrorCollector<S> errorCollector() {
            return nop;
        }

        @Override
        public Scope scope() {
            return CachedParseState.this.scope();
        }

        @Override
        public <T> T parse(NamedRule<S, T> rule) {
            ErrorCollector<S> saved = activeErrors;
            activeErrors = nop;
            try {
                return CachedParseState.this.parse(rule);
            } finally {
                activeErrors = saved;
            }
        }

        @Override
        public S input() {
            return CachedParseState.this.input();
        }

        @Override
        public int mark() {
            return CachedParseState.this.mark();
        }

        @Override
        public void restore(int mark) {
            CachedParseState.this.restore(mark);
        }

        @Override
        public Control acquireControl() {
            return CachedParseState.this.acquireControl();
        }

        @Override
        public void releaseControl() {
            CachedParseState.this.releaseControl();
        }

        @Override
        public ParseState<S> silent() {
            return this;
        }
    }

    static class SimpleControl implements Control {
        private boolean hasCut;

        @Override
        public void cut() {
            hasCut = true;
        }

        @Override
        public boolean hasCut() {
            return hasCut;
        }

        public void reset() {
            hasCut = false;
        }
    }
}

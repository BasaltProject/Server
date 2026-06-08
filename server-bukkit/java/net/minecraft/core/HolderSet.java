package net.minecraft.core;

import java.util.Iterator;
import java.util.List;
import net.minecraft.tags.TagKey;

public interface HolderSet<T> extends Iterable<Holder<T>> {
    int size();

    boolean contains(Holder<T> holder);

    abstract class ListBacked<T> implements HolderSet<T> {
        protected abstract List<Holder<T>> contents();

        @Override
        public int size() {
            return contents().size();
        }

        @Override
        public Iterator<Holder<T>> iterator() {
            return contents().iterator();
        }

        @Override
        public boolean contains(Holder<T> holder) {
            return contents().contains(holder);
        }
    }

    class Named<T> extends ListBacked<T> {
        private final TagKey<T> key;
        private List<Holder<T>> contents = List.of();

        public Named(TagKey<T> key) {
            this.key = key;
        }

        public TagKey<T> key() {
            return key;
        }

        public void bind(List<Holder<T>> holders) {
            contents = List.copyOf(holders);
        }

        @Override
        protected List<Holder<T>> contents() {
            return contents;
        }
    }
}

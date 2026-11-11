// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.thread;

import java.util.ArrayList;
import java.util.List;
import java.util.Queue;
import java.util.concurrent.ConcurrentLinkedQueue;

public interface StrictQueue<T extends Runnable> {
    Runnable pop();

    boolean push(T t);

    boolean isEmpty();

    int size();

    public static final class QueueStrictQueue implements StrictQueue<Runnable> {
        private final Queue<Runnable> queue;

        public QueueStrictQueue(Queue<Runnable> queue) {
            this.queue = queue;
        }

        @Override
        public Runnable pop() {
            return queue.poll();
        }

        @Override
        public boolean push(Runnable t) {
            return queue.add(t);
        }

        @Override
        public boolean isEmpty() {
            return queue.isEmpty();
        }

        @Override
        public int size() {
            return queue.size();
        }
    }

    public static record RunnableWithPriority(int priority, Runnable task) implements Runnable {
        @Override
        public void run() {
            task.run();
        }
    }

    public static final class FixedPriorityQueue implements StrictQueue<RunnableWithPriority> {
        private final List<Queue<Runnable>> levels = new ArrayList<>();

        public FixedPriorityQueue(int size) {
            for (int i = 0; i < size; i++) {
                levels.add(new ConcurrentLinkedQueue<>());
            }
        }

        @Override
        public Runnable pop() {
            for (Queue<Runnable> level : levels) {
                Runnable next = level.poll();
                if (next != null) {
                    return next;
                }
            }
            return null;
        }

        @Override
        public boolean push(RunnableWithPriority task) {
            int priority = task.priority();
            if (priority < 0 || priority >= levels.size()) {
                throw new IndexOutOfBoundsException(String.format(java.util.Locale.ROOT, "Priority %d not supported. Expected range [0-%d]", priority, levels.size() - 1));
            }
            return levels.get(priority).add(task);
        }

        @Override
        public boolean isEmpty() {
            return levels.stream().allMatch(Queue::isEmpty);
        }

        @Override
        public int size() {
            return levels.stream().mapToInt(Queue::size).sum();
        }
    }
}

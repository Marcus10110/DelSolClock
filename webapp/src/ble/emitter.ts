// Minimal typed event emitter — avoids a dependency and keeps the connection
// API event-driven like the MAUI service (StatusChanged / BatteryChanged etc.).

type Handler<T> = (payload: T) => void;

// Constraint allows plain `interface` event maps (which lack an implicit
// string index signature) as well as `Record`-shaped types.
export class Emitter<Events> {
  private handlers: { [K in keyof Events]?: Set<Handler<Events[K]>> } = {};

  on<K extends keyof Events>(event: K, handler: Handler<Events[K]>): () => void {
    (this.handlers[event] ??= new Set()).add(handler);
    return () => this.off(event, handler);
  }

  off<K extends keyof Events>(event: K, handler: Handler<Events[K]>): void {
    this.handlers[event]?.delete(handler);
  }

  protected emit<K extends keyof Events>(event: K, payload: Events[K]): void {
    this.handlers[event]?.forEach((h) => {
      try {
        h(payload);
      } catch (err) {
        console.error(`handler for "${String(event)}" threw`, err);
      }
    });
  }
}

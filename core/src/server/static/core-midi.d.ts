export {};
declare global {
  class CoreMidiManagement {
    constructor(send: (data: number[]) => void, ready?: (mode: string) => void,
                error?: (message: string) => void, now?: () => number);
    mode: 'sysex' | 'legacy' | null;
    static frame(operation: string): number[];
    reset(): void;
    start(): void;
    tick(): void;
    receive(data: Uint8Array | number[]): boolean;
    command(operation: string, argument?: number): void;
  }
}

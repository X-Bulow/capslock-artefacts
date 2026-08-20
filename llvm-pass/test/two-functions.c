__attribute__((noinline))
int helper(int value) {
    return value + 1;
}

int main(void) {
    return helper(41) == 42 ? 0 : 1;
}

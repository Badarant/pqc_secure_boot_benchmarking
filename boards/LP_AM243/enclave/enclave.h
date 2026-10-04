/*
 * enclave.h - AM243x M4F crypto enclave (DECISIONS.md -> D3, "inverted
 * architecture", LOCKED).
 *
 * A service that carries no state between requests: it owns no boot flow and
 * no enforcement. The R5F (`boot`) drives the whole flow and calls this
 * enclave over IPC (../common/enclave_ipc.h) for SHA256 and VERIFY.
 * App-agnostic and key-agnostic (R3): it hashes/verifies opaque
 * bytes at addresses the R5F provides, and the pubkey arrives in the VERIFY
 * request rather than being read from flash or compiled in.
 */
#ifndef PQSB_ENCLAVE_H
#define PQSB_ENCLAVE_H

/* Entry called from main() after driver init. Never returns: services
 * enclave_req_t/enclave_resp_t (../common/enclave_ipc.h) forever. */
void enclave_main(void *args);

#if PQSB_STACK_PAINT
/* Call as the first statement in main(): paints the unused MSP stack so
 * handleRequest() can report its high-water mark back to the R5F in
 * enclave_resp_t.stack_peak. See ../common/stack_paint.h. */
void enclave_stackPaint(void);
#endif

#endif /* PQSB_ENCLAVE_H */

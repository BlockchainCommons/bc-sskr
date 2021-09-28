# Security Review for `bc-shamir` [0.4.0](https://github.com/BlockchainCommons/bc-shamir/releases/tag/0.4.0) and `bc-sskr` [0.3.2](https://github.com/BlockchainCommons/bc-sskr/tree/0.3.2)

# `bc-shamir/bc-sskr` Security Review

A code audit of the [`bc-shamir`](https://github.com/BlockchainCommons/bc-shamir) and [`bc-sskr`](https://github.com/BlockchainCommons/bc-sskr) libraries was conducted in Summer 2021 by [Radically Open Security (ROS)](https://www.radicallyopensecurity.com/) to ensure the security of the libraries. The complete report is [archived here](reviews/2021-security-review.pdf). Our libraries were released as [bc-shamir v0.4.0](https://github.com/BlockchainCommons/bc-shamir/tree/0.4.0) and [bc-sskr v0.3.2](https://github.com/BlockchainCommons/bc-sskr/tree/0.3.2) in response to comments from ROS, to resolve various issues, as discussed below.

What follows is Blockchain Commons' commentary on issues reported by the audit. It is a complete listing of all issues in the audit that were marked as a `finding` — not marked as `non-finding` or recorded for `documentation`.

## Overview of Blockchain Commons Security Reviews

Blockchain Commons is dedicated to open infrastructure, but we cannot audit our own projects. Instead, we work with partners who are putting Blockchain Commons libraries and protocols into production usage so that they can fund security reviews for products that they are using which have not previously been reviewed. These reviews of the `bc-sskr` and `bc-shamir` libraries were funded by [Bitmark](https://bitmark.com/).

Blockchain Commons' review system shows the power of our open libraries: future companies will be able to depend on our Shamir and SSKR libraries thanks to Bitmark’s contribution, and then they’ll be able to make contributions of their own that will be much less than if they’d had to security review an entire suite of libraries themselves.

## Open Issues

### 1. `bc-shamir` provides only computational security [BTM-019]

The core methodology of Shamir's Secret Sharing provides information-theoretic security: it not only cannot be broken, even if an adversary has infinite computing power, but an attacker can gain no information on the secret. Blockchain Commons' implementation of Shamir's Secret Sharing, and thus also its own `bc-sskr` library, instead provides Computation Security: although vulnerable to the application of computing resources, it cannot be broken in a "reasonable amount" of time.

This results from the inclusion of four bytes of the HMAC-SHA256 digest of the secret inside each share, which are used to detect the submission of incorrect shares for reconstruction. 

Hashes are one-way functions: the secret can not be directly reconstructed from the hash. However this creates an oracle: with a brute-force computational attack, an adversary knowing T-1 shares could use the oracle to verify the correct secret by hashing possible secrets, but notably the oracle still has a false-positive rate of 2^-32. As a result, the cost of brute-forcing secrets interpolated with `bc-shamir` is still considered too high to be feasible and the number of false positives too overwhelming to be of any use, but the exact parameters are dependent on the number of unknown bits in the secret and the number of shares an adversary holds, as discussed in a possible attack, below.

_We consider this a definitional issue. It requires us to state that `bc-shamir` is Computationally Secure, not Information-Theoretic Secure. Since this is an equivalent level of security to many uses of Bitcoin and other cryptocurrencies that will integrate with `bc-shamir`, we consider it an appropriate and realistic boundary for the security required by a secret-sharding system._

### 2. Attackers with access to sufficient parts of a secret can brute force the rest [BTM-019]

This is the potential attack that is linked to the fact that `bc-shamir` provides Computational Security. Attackers with one less than the threshold of shares required for reconstruction (T-1) can reduce the possibilities for the last share, and thus determine the secret, through a brute force attack using the four-byte digest as an oracle. However, as noted, the search space is still very large, and we consider the security reasonable for any secret with an entropy of 128 bits or more.

If adversaries were able to gain partial information of the original secret through side channels, or through some flaw in the design, there would be more danger, but this requires portions of the secret to be leaked, and also for the remaining, unknown part of the secret to be sufficiently small that it could be brute-forced in a reasonable amount of time.

_We consider this to be a similar level of threat to any brute force attack. The secret could be discovered with the application of sufficient computing power, but it requires the prior revelation of all but a small amount of a secret before it would be reasonable on either current computers or on any non-quantum computers that could be produced in the foreseeable future, even with the reduction in search space resulting from holding almost-enough shares. Because our Shamir libraries were built to protect high-entropy secrets, we find this threat to be minimal except in the case of a catastrophic failure to protect the original secret, outside the bounds of the use of our libraries._

#### Final notes on open issues #1-2

_We have included discussions of these issues to be fully transparent on the security level of `bc-shamir` and `bc-sskr`. Nonetheless, the report from Radically Open Security lists this as a "low" threat and notes that it is "mainly of theoretical importance". Although they made proposals to restore the information-theoretic properties that originate with Shamir's Secret Sharing, we are instead adopting their prime suggestion, which is to document the design tradeoff._

_The decision to incorporate the fragments of the hashed secret was purposefully made to reduce complexity and share size for the sharding, and to allow the detection of malicious shares, and was weighed against the fact that `bc-shamir` and `bc-sskr` would be protecting high-entropy secrets such as seeds that would create an unreasonably high cost for a computational attack. This is a decision that is also mirrored in other popular secret-sharing protocols such as [SLIP-39](https://github.com/satoshilabs/slips/blob/master/slip-0039.md), who address this precise issue in their [design rationale](https://github.com/satoshilabs/slips/blob/master/slip-0039.md#Digest), noting that when one less than the threshold of shares are held, the entropy is effectively reduced by 32 bits, which remains sufficiently strong for entropy of 128 bits or more._

_When our libraries are used with Bitcoin or any other cryptography scheme that depends on computation security, we consider this to be a non-issue. In the case of Bitcoin, any address or public key can serve as an oracle and doesn't even require the attacker to hold shares, just the address!_

### 3. Sensitive data can be forced to leak by swap to disk [BTM-007]

If a local attacker forces the swap-out of a process running `bc-shamir`, it can leak sensitive information to disk.

_We are currently choosing to document this because we believe that it is a best practice for any software that handles sensitive information in a virtual memory environment. We certainly agree that information could leak in this situation, if local access on a machine were already attained. However, we generally believe that if someone has local access already to a machine, then the secret has a very high likelihood of being compromised, as that is the exact failure condition that we warn about when using Shamir's Secret Sharing: as the secret is being sharded or as it's being reconstructed, it is very vulnerable to local attacks on the machine where that is occurring._

_As a result, we consider this hardening suggestion worthy, but not critical. Though we have considered the use of `mlock` and `munlock` from the [Sodium library](https://libsodium.gitbook.io/doc/memory_management) as a methodology to keep sensitive information in memory, we are also looking into other solutions such as Bitcoin Core's sophisticated management of pools of locked memory. Any solutions may be limited on some systems and must thus therefore be managed carefully; further, our use of other libraries such as Libwally, which zero the memory of sensitive information, but which do not lock it, make any solutions we could manage on our own at this point limited._

## Closed Issues

All other issues reported as a `finding` by Radically Open Security have been resolved by Blockchain Commons and marked as `resolved` by Radically Open Security, as documented below. Our libraries [bc-shamir v0.4.0](https://github.com/BlockchainCommons/bc-shamir/tree/0.4.0) and [bc-sskr v0.3.2](https://github.com/BlockchainCommons/bc-sskr/tree/0.3.2) reflect this level of resolution.

### 1. Shamir: `split_secret()` had insufficient length checks [BTM-005]

There was insufficient input checking of the length of secrets to be split, which could lead to out-of-bound writes or segmentation faults. Resolved in `bc-shamir` [PR #31](https://github.com/BlockchainCommons/bc-shamir/pull/31/commits/dd01344c0fdcbc80264f63e70f90886448922ee9) and [PR #34](https://github.com/BlockchainCommons/bc-shamir/pull/34).

### 2. Shamir/SSKR: Use of `memset()` could be removed by compiler optimization [BTM-006]

The use of `memset` to sanitize sensitive data could have been removed by compiler optimization. It was replaced with `memzero` in [`bc-sskr`](https://github.com/BlockchainCommons/bc-sskr/commit/c40b37fe62e2257132952eee52197ffffef6533d) and [`bc-shamir`](https://github.com/BlockchainCommons/bc-shamir/commit/99f409f58258a836564b35279327ebe45f6829b5) commits.

### 3. General: Compiler hardening flags were missing [BTM-010]

Compiler hardening flags such as `-fstack-protector-all` and `D_FORTIFY_SOURCE=2` were missing. They have been added in [`bc-crypto-base` PR #35](https://github.com/BlockchainCommons/bc-crypto-base/pull/35), [`bc-shamir` PR #36](https://github.com/BlockchainCommons/bc-shamir/pull/36), and [`bc-sskr` PR #10](https://github.com/BlockchainCommons/bc-sskr/pull/10).

### 4. SSKR: Length parameter in `serialize_shard()` was not respected [BTM-015]

Though it did not create any current dangers, a lack of checking for the length parameter in `serialize_shard()` could have resulted in out-of-bounds writes in future code. This has been resolved in a [`bc-sskr` commit](https://github.com/BlockchainCommons/bc-sskr/commit/3c73b09f09f849481dbfaf6eee0bb56d0e9db1b9) plus [`bc-sskr` PR #11](https://github.com/BlockchainCommons/bc-sskr/pull/11).

### 5. SSKR: Out-of-bounds writes were possible in `generate_shard()` [BTM-018]

An out-of-bounds write was possible in `generate_shard()` due to insufficient checking of secret length. This was resolved in a [`bc-sskr` commit](https://github.com/BlockchainCommons/bc-sskr/commit/718a3ab28c2f025582928bbbd78551f4e8b4fdde) plus PRs [#11](https://github.com/BlockchainCommons/bc-sskr/pull/11) and [#12](https://github.com/BlockchainCommons/bc-sskr/pull/12).

### 6. Shamir: `split_secret()` did not sufficiently check parameters [BTM-020]

The `split_secret()` function did not sufficiently check `threshold` and `shard_count` parameters. This has been fixed as part of [`bc-shamir` PR #37](https://github.com/BlockchainCommons/bc-shamir/pull/37).

### 7. Shamir: Buffer overflow was possible in `hazmat_lagrange_basis()` [BTM-021]

A buffer overflow would occur in `hazmat_lagrange_basis` if `n > 24`. This was also resolved as part of [`bc-shamir` PR #37](https://github.com/BlockchainCommons/bc-shamir/pull/37).

### 8. Shamir: `recover_secret()` was missing parameter checks [BTM-022]

Similar to the issue with `split_secret`, there were insufficient checks for `threshold` and `shard_count` in `recover_secret()`. Resolved in [`bc-shamir` PR #38](https://github.com/BlockchainCommons/bc-shamir/pull/38).



New Storage Daemon encryption and XXH64 checksum
++++++++++++++++++++++++++++++++++++++++++++++++

This new version switch from the ``BB02`` volume format to the new ``BB03``.
This new format replaces the old 32bits CRC32 checksum with
the new and faster 64bits *XXH64*.
The new format also add support for volume encryption by the Storage Daemon.
Finally this patch add 3 new file Digest Algorithm *XXH64*,
*XXH3_64* and *XXH3_128* to the ones that already exists *MD5*, *SHA1*, *SHA256*,
*SHA512*. These 3 new *XX...* algorithms are not Cryptographic algorithms,
but are very good regarding the dispersion and randomness qualities.
There are also the fastest ones available.

The New XXH64 checksum
======================

The new XXH64 checksum, is optional you can disable it using the same
directive than for the old CRC32 in ``BB02`` ``Block Checksum = no``

Storage Daemon encryption
=========================

Blocks in the volumes can be encrypted using the
*BLOCK_CIPHER_AES_128_XTS* or *BLOCK_CIPHER_AES_256_XTS* ciphers algorithms.
These symmetrical ciphers are fast and used by most applications
that need to do symmetrical encryption of blocks.

Every blocks are encrypted using a *key* that is unique for the
volume and an *IV* (Initialization vector) that is the block number
that is saved in the block header.
The *XTS's* ciphers are specifically designed to support an *IV* with a low
entropy

The first *block* of the volume that hold the *Volume Label*
is not encrypted because some fields as the *volume name*
are required to manage the volume and the encryption.
The user has the option to obfuscate some fields
that are not required and could hold critical
information like for example the *hostname*.
These field are replaced by the string *"OBFUSCATED"*

The header of the block are not encrypted. This 24 bytes header
don't hold any user information. Here is the content of the header:

- the 32bits header option bit field
- the 32bits block length
- the 32bits block number
- the ``BB03`` string
- the 32bits volume session id
- the 32bits volume session time

The *volume session time* is the time of the Storage Daemon
at startup and not the time of the backup.
The *volume session id* is reset at zero when the daemon starts
and is incremented at every backup by the Storage Daemon

The 64bits *XXH64* checksum is encrypted with the data.
The *block* must be be decrypted and then the checksum can be verified.
If the checksum matches, you can be confident that you have the right
encryption key and that the block has not been modified.
The drawback is that it is not possible to verify the integrity of
the block without the encryption key.

The data in the *data spool* are not encrypted, don't store you
data spool on an unsafe storage!

The new Storage Daemon Directives
=================================

Here are the new directives in the Storage resource of the Storage Daemon

.. _Storage:Device:BlockEncryption:

**Block Encryption = <none|enable|strong>**
   This directive allows you to enable the encryption for the given device.
   The encryption can be of 3 different types:

   **none** This is the default, the device don't do any encryption.

   **enable** The device encrypts the data but let all information in
   the *volume label* in clear text.

   **strong** The device encrypts the data, and obfuscate any
   information in the *volume label* except the one that are needed
   for the management of the volume. The fields that are obfuscate are:
   *volume name*.

.. _Storage:Storage:EncryptionCommand:

**Encryption Command = <command>** The **command** specifies an external
   program that must provide the key related to a volume.


The Encryption Command
======================

The *Encryption Command* is called by the Storage Daemon every time it
initialize a new volume or mount an existing one using a device
with encryption enable.
We are providing a very simple script that can handle keys
Here is an example about how to call the command.

::

   EncryptionCommand = "/opt/bacula/script/sd_encryption_command.py getkey --key-dir /opt/bacula/working/keys"

Be careful the command is limited to 127 characters.
The same variable substitution than for the *AutoChanger command* are provided.

The program can be an interface with your existing key management system or
do the key management on its own.
*Bacula* provides a sample script *sd_encryption_command.py* that do the work.

Lets illustrate our protocol with some usage of our script::

   $ OPERATION=LABEL VOLUME_NAME=Volume0001 ./sd_encryption_command.py getkey --cipher AES_128_XTS --key-dir tmp/keys 
   cipher: AES_128_XTS
   cipher_key: G6HksAYDnNGr67AAx2Lb/vecTVjZoYAqSLZ7lGMyDVE=
   volume_name: Volume0001

   $ OPERATION=READ VOLUME_NAME=Volume0001 ./sd_encryption_command.py getkey --cipher AES_128_XTS --key-dir tmp/keys 
   cipher: AES_128_XTS
   cipher_key: G6HksAYDnNGr67AAx2Lb/vecTVjZoYAqSLZ7lGMyDVE=
   volume_name: Volume0001

   $ cat tmp/keys/Volume0001 
   cipher: AES_128_XTS
   cipher_key: G6HksAYDnNGr67AAx2Lb/vecTVjZoYAqSLZ7lGMyDVE=
   volume_name: Volume0001

   $ OPERATION=READ VOLUME_NAME=DontExist ./sd_encryption_command.py getkey --cipher AES_128_XTS --key-dir tmp/keys 2>/dev/null
   error: no key information for volume "DontExist"
   $ echo $?
   0

   $ OPERATION=BAD_CMD VOLUME_NAME=Volume0002 ./sd_encryption_command.py getkey --cipher AES_128_XTS --key-dir tmp/keys 2>/dev/null
   error: environment variable OPERATION invalid "BAD_CMD" for volume "Volume0002"
   $ echo $?
   0

You can see in this command above that we keep the keys into one
directory *tmp/keys*, we pass the arguments using the *environment variables*.

*Bacula* pass the following variables via the *environment*:

   **OPERATION** This is can *LABEL* when the volume is labeled, in this case
   the script should generate a new key or this can be *READ* when
   the volume has already a label and the Storage Daemon need the already
   existing key to read or append data to the volume

   **VOLUME_NAME** This is the name of the volume

Some variables are already there to support a *Master Key* in the future.
This feature is not yet supported, but will come later:

   **ENC_CIPHER_KEY** This is a base64 encoded version of the key encrypted by
   the *master key*

   **MASTER_KEYID** This is a base64 encoded version of the key Id of 
   the *master key* that was used to encrypt the *ENC_CIPHER_KEY* above.

*Bacula* expects some values in return:

   **volumename** This is a repetition of the name of the volume that is
   given to the script. This field is optional and ignored by Bacula.

   **cipher** This is the cipher that Bacula must use. 
   Bacula knows the following ciphers: *AES_128_XTS* and *AES_256_XTS*.
   Of course the key length vary with the cipher.

   **cipher_key** This is the symmetric *key* in *base 64* format.

   **comment** This is a single line of comment that is optional and ignored by Bacula.

   **error** This is a single line error message.
   This is optional, but when provided, Bacula consider that the script
   returned an error and display this error in the job log.

Bacula expect an *exit code" of 0, if the script exits with a different
error code, any output are ignored and Bacula display a generic message
with the exit code in the job log.
To return an error to bacula, the script must use the *error* field
and return an error code of 0.

What is encrypted and what is not
=================================

The main goal of encryption is to prevent anybody that don't own the key to
read the data. And *Bacula* does it well. But encryption alone don't protect
again some modification.

The first block of the volume is the *volume label* and it is not encrypted.
Some information are required for the management of the *volume* itself.
The only data in the *volume label* coming from the user are: the *hostname*,
the *volumename* *poolname*. The *hostname* can be obfuscated using
the *STRONG* mode of the encryption, the *poolname* and the *volumename*
could be made useless to a attacker by using generic name lie ``PoolAlpha`` or
``Volume12345``.

Also be aware that data in the catalog: the directories, filenames, and
the *JobLog* are not encrypted.

An attacker could make some undetected modification to the volume.
The easiest way is to remove one block inside the volume.
Other verification inside *Bacula* could detect such modification and
an attacker must be meticulous, but it is possible.

The *XXH64* checksum inside each volume are encrypted using the encryption key.
This is not as good as using a certified signature but this provides
substantial confidence that the block will not be modified easily.

To resume, you can be confident that:

- An attacker cannot read any of your data: **Very Strong**
- An attacker cannot substitute the volume by another one: **Strong**
- An attacker cannot modify the content of the volume: **Good**


Notes for the developers and support and personal
+++++++++++++++++++++++++++++++++++++++++++++++++

The new Volume Format
=====================
In the new ``BB03`` volume format, the space used by the CRC32 is now
used by a 32bits bit field that for now only knows about the
two *option* bits:

::

  BLKHOPT_CHKSUM = 1 << 0,    // If the XXH64 checksum is enable
  BLKHOPT_ENCRYPT_VOL = 1 << 1,   // If the volume is encrypted
  BLKHOPT_ENCRYPT_BLOCK = 1 << 2,   // If this block is encrypted

Notice that the *block number* is reset to zero when the volume is happened
and then is not unique! This could be a little security concern,
as multiple block could be encrypted using the same key and *IV*

The New XXH64 checksum
======================

The new XXH64 checksum, is saved just after *VolSessionTime* and is set to
zero if the checksum is disabled. The checksum is calculated on the all
block with the new 64bits checksum field set to zero.
To verify the checksum
you must first verify that *the BLKHOPT_CHKSUM* bit is enable, then un-serialize
the value of the checksum in the block and save it, then replace it with zeroes,
calculate the checksum of the block and compare it with the value that
you have saved.


Volume Signature
================

The purpose is to be able to verify:
 - the authenticity of the volume (that we have the volume we are looking for)
 - the integrity of the volume (that the data on the volume did not change)

This is independent of the volume encryption.

Some assertion to validate:
 - The Volume Signature is configured on the SD side only.
 - Nothing is configured on the DIR side
 - The SD should be able to tell the DIR that this feature is enable on one volume,
   and the DIR **could** (something to decide) store this information in the *volume media* in the catalog
 - The DIR must be able to initiate a volume signature check and display the result.
   We need to define an interfece for that.
 - The signature are more commonly done using public/private keys pairs.
 - The customer could appreciate to use different keys pairs for different groups of volumes
   (for example regarding the poolname)
 - The KeyManager can handle these keys the same way it is handling the encryption keys.

Something in the volume must tell the user which key was used to sign the volume and which one can be used to verify:
 - this information is not mandatory as the user could keep this information somewhere else
 - we could store the keyid, the KeyManager can then provide the public key
 - we can store the public key. A 4096bits public key is about 717bytes width (this include the *modulus*).
   But this is not valid has me must validate the content of the volume from information from outer the volume.
   Then the keyid is better solution, we are sure that the public key will come from outside.

At the block level.

The signature digest for a SHA256 and a 2048bits key is 122bytes.
I don't know how the size fluctuate, but the size could vary if we want to use another
hash function or maybe another key size.
Then the size in the block must be variable.
The best place at the end of the header.
How to combine the signature and the XXH64 checksum ?

First set the signature and the XXH64 checksum spaces to 0x00.
Calculate and fill in the signature area.
Second calculate and fill in the XXH64 checksum area.
For the verification, the XXH64 checksum will always be verified while
the signature will only be verified if we can provide or verify
the authenticity of the public key.



Weakness of the signature in bacula
===================================

The signature sign individual block.
If one block is removed from the volume we don't know it.
We could ensure that the BlockNumber is well updated
and verify that there is no missing BlockNumber.
Today the BlockNumber is reset to zero every time the volume is unmounted.
In fact when it is mounted again we don't read the value of the last BlockNumber
and restart at zero.
Even with *valid* BlockNumber we are not safe.
The last blocks of the volume could be removed and we will not know.
For that we should refresh the volume label with this information.
This is impossible with some device like a tape.

Worst it is possible to swap some block inside a file.
An attacker could recompose a file by switching and dropping some blocks of the same file.

This make our signature very weak. We only secure the front door.
Do we still need to implement a signature?
We don't need to decide today. What I'm doing today will have a little
impact, not more than 4 extra bytes in the volume label that could
be recycled for the next feature.

The signature, checksum64 and encryptions together
==================================================

The question is how to order the 3 operation at backup and restore time::

  uint32_t header_option; // was checksum in BB02
  uint32_t block_len
  uint32_t BlockNumber
  char ID[4] // BB03
  uint32_t VolSessionId
  uint32_t VolSessionTime
  uint64_t CheckSum64
  if (header_option | BLKHOPT_SIGN_BLOCK)
  {
    uint32_t signature_size
    char signature[signature_size]
  }
  char PAYLOAD[]

notice that the checksum64 and the signature are in the middle of the block
the only way for them to protect the entire block it to reset these
area to zero before to calculate them.

(choice 1)
CheckSum64 should be like a hardware checksum.
It should protect all the block, be computed last (after encryption).
At read time it should be checked first. (before decryption)
if we use the wrong decryption key, we will have a block
totally messed up and we will not know
and maybe some crash while the SD try to decode and use the *random* data

(choice 2)
But It can be done in a different way!
Having the CheckSum64 calculated BEFORE the encryption.
would validate that we have the right key to decrypt the block.
In this case it should be calculated before the encryption
and at read time it should be used after decryption.
In this situation the key is required to verify that there
is no "hardware" error, but reading the volume without the right
key is useless.
This is the choice I made at first
I we are using the wrong key at restore time, the CheckSum64
will detect it and bacula will not try to decode a corrupted block

About the signature.
The signature is two time optional:
- we don't need to use a signature
- if we have it in a volume, we don't need to verify it and we can still read the volume.

The signature must be independent of encryption
we must be able to verify the signature without having the key to decrypt the data.
Then the signature must be calculated AFTER the encryption
At read time it must be verified before decryption.

In choice1::

    do encryption
    set checksum64 to zero
    set signature to zero
    calculate and fill in signature
    calculate and fill in checksum64

At restore time (choice1)::

    read and copy checksum64
    set checksum64 to zero
    calculate & verify checksum64
    read and copy signature
    set signature to zero
    calculate & verify signature  <== we can stop here if we just want to check the signature
    do decryption 

In choice2::

    set signature to zero
    set checksum64 to zero
    calculate and fill in checksum64
    do encryption
    set signature to zero one more time
    calculate and fill in signature

At restore time (choice2)::

    read and copy signature
    set signature to zero
    calculate & verify signature <== we can stop here if we just want to check the signature
    do decryption
    set signature to zero one more time
    read and copy checksum64
    set checksum64 to zero
    calculate & verify checksum64

Despite the *"set signature to zero one more time"*, and the fact that the CheckSum64
don't work like a hardware checksum (that anybody can check), I prefer (choice2)

Signatures
==========
Because we don't know in advance on which volume a block will be written,
and because the space for the signature must be reserved before to 
start writing the records inside the volume, all the blocks written by a
device must share the same signature size.
Then some signature parameters must be defined at the device level.
The Key manager can still provide the key for the signature, but
they must match the device parameters!

The signature should not prevent the volume to be read on any other
device or machine. The information inside the volume, that describe
the structure of the volume must be self sufficient to read the volume.

If we want to change the configuration of the device, for example
to increase the "security" by increasing the hash or the key size,
the old volumes should be able to be read on this device.



https://medium.com/@bn121rajesh/rsa-sign-and-verify-using-openssl-behind-the-scene-bf3cac0aade2
https://github.com/bn121rajesh/ipython-notebooks/blob/master/BehindTheScene/OpensslSignVerify/openssl_sign_verify.ipynb

